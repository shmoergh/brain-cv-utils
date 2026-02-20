#include "slew-limiter.h"

#include <pico/time.h>

namespace {
float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

float pot_to_slew_rate(uint8_t pot_value) {
	if (pot_value == 0) return 0.0f;
	float x = static_cast<float>(pot_value) / 255.0f;
	return x * x * x;
}
}

SlewLimiter::SlewLimiter()
	: current_ch1_(0.0f),
	  current_ch2_(0.0f),
	  last_time_us_(0),
	  linked_(false),
	  button_b_prev_(false) {}

void SlewLimiter::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
						  brain::io::AudioCvOut& cv_out,
						  Calibration& calibration, bool button_b_pressed,
						  brain::ui::Leds& leds, LedController& led_controller) {
	(void)calibration;

	// Button B release: toggle linked mode
	if (button_b_prev_ && !button_b_pressed) {
		linked_ = !linked_;
	}
	button_b_prev_ = button_b_pressed;

	// Delta time
	uint32_t now_us = time_us_32();
	uint32_t dt_us = now_us - last_time_us_;
	last_time_us_ = now_us;
	if (dt_us > 100000) dt_us = 100000;

	// Pots
	float rise_rate = pot_to_slew_rate(pots.get(kPotRise));
	float fall_rate = linked_ ? rise_rate : pot_to_slew_rate(pots.get(kPotFall));
	float shape = static_cast<float>(pots.get(kPotShape)) / 255.0f;

	// Slew coefficients
	float rise_coeff = rise_rate > 0.001f
		? clampf(static_cast<float>(dt_us) / (rise_rate * static_cast<float>(kMaxSlewUs)), 0.0f, 1.0f)
		: 1.0f;
	float fall_coeff = fall_rate > 0.001f
		? clampf(static_cast<float>(dt_us) / (fall_rate * static_cast<float>(kMaxSlewUs)), 0.0f, 1.0f)
		: 1.0f;

	// Read inputs and apply slew
	float in_ch1 = cv_in.get_voltage_channel_a();
	float in_ch2 = cv_in.get_voltage_channel_b();
	current_ch1_ = slew_channel(in_ch1, current_ch1_, rise_coeff, fall_coeff, shape);
	current_ch2_ = slew_channel(in_ch2, current_ch2_, rise_coeff, fall_coeff, shape);

	// Output
	const float out_a_voltage = clampf(current_ch1_, 0.0f, kMaxVoltage);
	const float out_b_voltage = clampf(current_ch2_, 0.0f, kMaxVoltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_a_voltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_b_voltage);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
}

float SlewLimiter::slew_channel(float input, float current, float rise_coeff,
								float fall_coeff, float shape) {
	float diff = input - current;
	if (diff == 0.0f) return current;

	float coeff = diff > 0.0f ? rise_coeff : fall_coeff;

	// Exponential: fraction of remaining distance
	float exp_step = coeff * diff;

	// Linear: constant rate
	float sign = diff > 0.0f ? 1.0f : -1.0f;
	float linear_move = coeff * 10.0f * sign;
	if ((diff > 0.0f && linear_move > diff) || (diff < 0.0f && linear_move < diff)) {
		linear_move = diff;
	}

	// Blend: shape=0 → linear, shape=1 → exponential
	float step = linear_move * (1.0f - shape) + exp_step * shape;
	return current + step;
}
