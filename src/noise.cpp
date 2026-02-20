#include "noise.h"

#include "pico/time.h"

Noise::Noise()
	: rng_state_(123456789),
	  frozen_(false) {
	ch_a_.last_update_us = 0;
	ch_a_.current_value = kDacMax / 2;
	ch_b_.last_update_us = 0;
	ch_b_.current_value = kDacMax / 2;
}

uint32_t Noise::next_random(uint32_t seed) {
	// xorshift32
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

uint32_t Noise::pot_to_interval_us(uint8_t pot_value) {
	// Logarithmic mapping: 0 = fastest (~1ms), 255 = slowest (~2s)
	// Using integer approximation of exponential curve
	if (pot_value == 0) return kMinIntervalUs;

	// Map 0-255 to kMinIntervalUs..kMaxIntervalUs logarithmically
	// exp curve: min * (max/min)^(pot/255)
	// Approximate with piecewise: quadratic * linear blend
	uint32_t pot32 = static_cast<uint32_t>(pot_value);
	uint32_t range = kMaxIntervalUs - kMinIntervalUs;

	// Cubic-ish curve for nice log feel: (pot/255)^3 * range
	uint32_t t = (pot32 * pot32 * pot32) / (255UL * 255UL);  // 0..255
	uint32_t interval = kMinIntervalUs + (t * range) / 255UL;

	if (interval > kMaxIntervalUs) interval = kMaxIntervalUs;
	return interval;
}

void Noise::update(brain::ui::Pots& pots, brain::io::AudioCvOut& cv_out,
				   bool button_b_pressed, brain::ui::Leds& leds,
				   LedController& led_controller) {
	uint32_t now = time_us_32();

	// Button B toggles freeze
	frozen_ = button_b_pressed;

	if (!frozen_) {
		uint32_t interval_a = pot_to_interval_us(pots.get(kPotSpeedA));
		uint32_t interval_b = pot_to_interval_us(pots.get(kPotSpeedB));

		// Channel A
		uint32_t elapsed_a = now - ch_a_.last_update_us;
		if (elapsed_a >= interval_a) {
			rng_state_ = next_random(rng_state_);
			ch_a_.current_value = static_cast<uint16_t>(rng_state_ & 0x0FFF);  // 0..4095
			ch_a_.last_update_us = now;
		}

		// Channel B
		uint32_t elapsed_b = now - ch_b_.last_update_us;
		if (elapsed_b >= interval_b) {
			rng_state_ = next_random(rng_state_);
			ch_b_.current_value = static_cast<uint16_t>(rng_state_ & 0x0FFF);  // 0..4095
			ch_b_.last_update_us = now;
		}
	}

	// Output: convert DAC value (0-4095) to voltage (0.0-10.0V)
	float voltage_a = static_cast<float>(ch_a_.current_value) * 10.0f / static_cast<float>(kDacMax);
	float voltage_b = static_cast<float>(ch_b_.current_value) * 10.0f / static_cast<float>(kDacMax);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, voltage_a);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, voltage_b);

	// VU display
	led_controller.render_output_vu(leds, ch_a_.current_value, ch_b_.current_value);
}
