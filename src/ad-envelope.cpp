#include "ad-envelope.h"

#include <pico/time.h>

#include "fixed-point.h"

AdEnvelope::AdEnvelope()
	: stage_(Stage::kIdle),
	  envelope_q15_(0),
	  stage_start_us_(0),
	  stage_duration_us_(0),
	  button_b_prev_(false),
	  pulse_triggered_(false) {}

void AdEnvelope::init(brain::io::Pulse& pulse) {
	pulse.on_rise([this]() {
		pulse_triggered_ = true;
	});
}

void AdEnvelope::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
						 brain::io::AudioCvOut& cv_out, brain::io::Pulse& pulse,
						 Calibration& calibration, bool button_b_pressed,
						 brain::ui::Leds& leds, LedController& led_controller) {
	(void)calibration;

	pulse.poll();

	uint32_t now_us = time_us_32();

	// Read pots
	uint32_t attack_us = pot_to_time_us(pots.get(kPotAttack));
	uint32_t decay_us = pot_to_time_us(pots.get(kPotDecay));
	uint16_t shape_q15 = fixed_point::u8_to_q15(pots.get(kPotShape));

	// CV modulation: CV In A modulates attack time (±50%)
	// get_voltage returns -10..+10V range, center at 0V
	// Map: 0V = no modulation, +5V = halve time, -5V = double time
	float cv_mod = cv_in.get_voltage_channel_a();
	if (cv_mod > 0.5f || cv_mod < -0.5f) {
		// Scale factor: 1.0 at 0V, 0.5 at +5V, 2.0 at -5V
		float scale = 1.0f - (cv_mod / 10.0f);
		if (scale < 0.1f) scale = 0.1f;
		if (scale > 4.0f) scale = 4.0f;
		attack_us = static_cast<uint32_t>(static_cast<float>(attack_us) * scale);
		if (attack_us < kMinTimeUs) attack_us = kMinTimeUs;
		if (attack_us > kMaxTimeUs) attack_us = kMaxTimeUs;
	}

	// Trigger detection: Button B press or Pulse In rising edge
	bool trigger = false;
	if (!button_b_prev_ && button_b_pressed) {
		trigger = true;
	}
	button_b_prev_ = button_b_pressed;

	if (pulse_triggered_) {
		trigger = true;
		pulse_triggered_ = false;
	}

	// Handle trigger: always restart from attack
	if (trigger) {
		stage_ = Stage::kAttack;
		stage_start_us_ = now_us;
		stage_duration_us_ = attack_us;
	}

	// Process envelope stages
	switch (stage_) {
		case Stage::kIdle:
			envelope_q15_ = 0;
			break;

		case Stage::kAttack: {
			uint32_t elapsed_us = now_us - stage_start_us_;
			if (elapsed_us >= stage_duration_us_) {
				// Attack complete, transition to decay
				envelope_q15_ = kQ15One;
				stage_ = Stage::kDecay;
				stage_start_us_ = now_us;
				stage_duration_us_ = decay_us;
			} else {
				// Linear position 0..kQ15One
				int32_t linear_q15 = static_cast<int32_t>(
					(static_cast<int64_t>(elapsed_us) * kQ15One) / stage_duration_us_);
				envelope_q15_ = apply_shape(linear_q15, shape_q15, true);
			}
			break;
		}

		case Stage::kDecay: {
			uint32_t elapsed_us = now_us - stage_start_us_;
			if (elapsed_us >= stage_duration_us_) {
				// Decay complete, back to idle
				envelope_q15_ = 0;
				stage_ = Stage::kIdle;
				// EOC trigger: pulse output
				pulse.set(true);
			} else {
				// Linear position kQ15One..0 (inverted)
				int32_t linear_q15 = kQ15One - static_cast<int32_t>(
					(static_cast<int64_t>(elapsed_us) * kQ15One) / stage_duration_us_);
				envelope_q15_ = apply_shape(linear_q15, shape_q15, false);
			}
			// Clear EOC pulse after a short time
			if (elapsed_us > 0) {
				pulse.set(false);
			}
			break;
		}
	}

	// Clamp
	envelope_q15_ = fixed_point::clamp_i32(envelope_q15_, 0, kQ15One);

	// Convert to voltage: 0..kQ15One → 0.0..10.0V
	// Output on both channels (same envelope)
	float out_voltage = static_cast<float>(envelope_q15_) * 10.0f / static_cast<float>(kQ15One);

	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_voltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_voltage);
	led_controller.render_output_vu(leds, out_voltage, out_voltage);
}

uint32_t AdEnvelope::pot_to_time_us(uint8_t pot_value) {
	if (pot_value == 0) return kMinTimeUs;
	// Cubic log curve: x^3 mapped to kMinTimeUs..kMaxTimeUs
	uint32_t x = static_cast<uint32_t>(pot_value);
	// x^3 / 255^3 * range + min
	// Use 64-bit to avoid overflow: (x*x*x) can be up to 255^3 = 16581375
	uint64_t cubed = static_cast<uint64_t>(x) * x * x;
	uint64_t max_cubed = 255ULL * 255 * 255;
	return static_cast<uint32_t>(kMinTimeUs + (cubed * (kMaxTimeUs - kMinTimeUs)) / max_cubed);
}

int32_t AdEnvelope::apply_shape(int32_t linear_pos_q15, uint16_t shape_q15, bool is_attack) {
	// Linear component: just the position as-is
	// Exponential component: for attack, curve upward (slow start, fast end)
	//                        for decay, curve downward (fast start, slow end)
	// Approximate exponential with x^2 (cheaper than real exp)

	int32_t exp_pos_q15;
	if (is_attack) {
		// Exponential attack: x^2 (slow start, fast finish)
		exp_pos_q15 = fixed_point::mul_q15(linear_pos_q15,
										   static_cast<uint16_t>(linear_pos_q15));
	} else {
		// Exponential decay: 1 - (1-x)^2 (fast start, slow finish)
		int32_t inv = kQ15One - linear_pos_q15;
		int32_t inv_sq = fixed_point::mul_q15(inv, static_cast<uint16_t>(inv));
		exp_pos_q15 = kQ15One - inv_sq;
	}

	// Blend linear and exponential based on shape pot
	return fixed_point::blend_q15(linear_pos_q15, exp_pos_q15, shape_q15);
}
