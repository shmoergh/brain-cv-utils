#include "ad-envelope.h"

#include <pico/time.h>

#include "fixed-point.h"

namespace {
constexpr float kEnvelopePeakSignalV = 5.0f;	// Envelope signal domain: 0..+5V.
constexpr float kCenterVoltageV = 5.0f;		// DAC domain center for 0V signal.
constexpr float kMaxDacVoltageV = 10.0f;
constexpr float kGateThresholdV = 1.0f;
}

AdEnvelope::AdEnvelope()
	: envelope_a_{Stage::kIdle, 0, 0, 0, false},
	  envelope_b_{Stage::kIdle, 0, 0, 0, false},
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

	// Trigger detection: per-channel gate rising edges, manual button, and pulse-in.
	bool trigger_a = false;
	bool trigger_b = false;
	const bool gate_a_high = cv_in.get_voltage_channel_a() > kGateThresholdV;
	const bool gate_b_high = cv_in.get_voltage_channel_b() > kGateThresholdV;
	if (!envelope_a_.gate_prev_high && gate_a_high) {
		trigger_a = true;
	}
	if (!envelope_b_.gate_prev_high && gate_b_high) {
		trigger_b = true;
	}
	envelope_a_.gate_prev_high = gate_a_high;
	envelope_b_.gate_prev_high = gate_b_high;

	if (!button_b_prev_ && button_b_pressed) {
		trigger_a = true;
		trigger_b = true;
	}
	button_b_prev_ = button_b_pressed;

	if (pulse_triggered_) {
		trigger_a = true;
		trigger_b = true;
		pulse_triggered_ = false;
	}

	if (trigger_a) {
		trigger_envelope(envelope_a_, now_us, attack_us);
	}
	if (trigger_b) {
		trigger_envelope(envelope_b_, now_us, attack_us);
	}

	const bool eoc_a = process_envelope(envelope_a_, now_us, decay_us, shape_q15);
	const bool eoc_b = process_envelope(envelope_b_, now_us, decay_us, shape_q15);
	pulse.set(eoc_a || eoc_b);

	// Clamp and convert unipolar envelope signal (0..+5V) into DAC domain around +5V center.
	envelope_a_.envelope_q15 = fixed_point::clamp_i32(envelope_a_.envelope_q15, 0, kQ15One);
	envelope_b_.envelope_q15 = fixed_point::clamp_i32(envelope_b_.envelope_q15, 0, kQ15One);
	const float envelope_a_signal_v = static_cast<float>(envelope_a_.envelope_q15) *
									  kEnvelopePeakSignalV / static_cast<float>(kQ15One);
	const float envelope_b_signal_v = static_cast<float>(envelope_b_.envelope_q15) *
									  kEnvelopePeakSignalV / static_cast<float>(kQ15One);
	float out_a_voltage = envelope_a_signal_v + kCenterVoltageV;
	float out_b_voltage = envelope_b_signal_v + kCenterVoltageV;
	if (out_a_voltage < 0.0f) out_a_voltage = 0.0f;
	if (out_a_voltage > kMaxDacVoltageV) out_a_voltage = kMaxDacVoltageV;
	if (out_b_voltage < 0.0f) out_b_voltage = 0.0f;
	if (out_b_voltage > kMaxDacVoltageV) out_b_voltage = kMaxDacVoltageV;

	cv_out.set_voltage_calibrated(brain::io::AudioCvOutChannel::kChannelA, out_a_voltage);
	cv_out.set_voltage_calibrated(brain::io::AudioCvOutChannel::kChannelB, out_b_voltage);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
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

void AdEnvelope::trigger_envelope(EnvelopeState& state, uint32_t now_us, uint32_t attack_us) {
	state.stage = Stage::kAttack;
	state.stage_start_us = now_us;
	state.stage_duration_us = attack_us;
}

bool AdEnvelope::process_envelope(EnvelopeState& state, uint32_t now_us, uint32_t decay_us,
								  uint16_t shape_q15) {
	switch (state.stage) {
		case Stage::kIdle:
			state.envelope_q15 = 0;
			return false;

		case Stage::kAttack: {
			uint32_t elapsed_us = now_us - state.stage_start_us;
			if (elapsed_us >= state.stage_duration_us) {
				// Attack complete, transition to decay.
				state.envelope_q15 = kQ15One;
				state.stage = Stage::kDecay;
				state.stage_start_us = now_us;
				state.stage_duration_us = decay_us;
			} else {
				int32_t linear_q15 = static_cast<int32_t>(
					(static_cast<int64_t>(elapsed_us) * kQ15One) / state.stage_duration_us);
				state.envelope_q15 = apply_shape(linear_q15, shape_q15, true);
			}
			return false;
		}

		case Stage::kDecay: {
			uint32_t elapsed_us = now_us - state.stage_start_us;
			if (elapsed_us >= state.stage_duration_us) {
				// Decay complete, back to idle.
				state.envelope_q15 = 0;
				state.stage = Stage::kIdle;
				return true;
			}
			int32_t linear_q15 = kQ15One - static_cast<int32_t>(
				(static_cast<int64_t>(elapsed_us) * kQ15One) / state.stage_duration_us);
			state.envelope_q15 = apply_shape(linear_q15, shape_q15, false);
			return false;
		}
	}
	return false;
}
