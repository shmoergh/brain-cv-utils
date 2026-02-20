#include "led-controller.h"

namespace {
float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

uint8_t vu_brightness(float magnitude, float zone_index, float zone_size) {
	return static_cast<uint8_t>(
		clampf((magnitude - zone_index * zone_size) * 255.0f / zone_size, 0.0f, 255.0f));
}
}  // namespace

void LedController::start_mode_change(uint32_t now_us) {
	mode_led_override_started_us_ = now_us;
	mode_led_override_until_us_ = now_us + kModeLedHoldUs;
}

bool LedController::is_mode_override_active(uint32_t now_us) const {
	return static_cast<int32_t>(now_us - mode_led_override_until_us_) < 0;
}

void LedController::render_mode_change(brain::ui::Leds& leds, uint8_t mode_index,
									   uint8_t num_modes, uint32_t now_us) const {
	const uint32_t elapsed = now_us - mode_led_override_started_us_;
	const uint32_t phase = elapsed / kModeLedBlinkHalfPeriodUs;
	const bool led_on = (phase % 2u) == 0u;

	if (!led_on) {
		leds.off_all();
		return;
	}

	for (uint8_t i = 0; i < num_modes; i++) {
		if (i == mode_index) {
			leds.on(i);
		} else {
			leds.off(i);
		}
	}
}

void LedController::render_output_vu(brain::ui::Leds& leds, float out_a_voltage,
									 float out_b_voltage) const {
	// Output domain is 0..10V with 5V as bipolar center.
	const float mag_a = clampf(out_a_voltage >= 5.0f ? out_a_voltage - 5.0f : 5.0f - out_a_voltage, 0.0f, 5.0f);
	const float mag_b = clampf(out_b_voltage >= 5.0f ? out_b_voltage - 5.0f : 5.0f - out_b_voltage, 0.0f, 5.0f);
	const float kZone = 5.0f / 3.0f;

	leds.set_brightness(0, vu_brightness(mag_a, 0.0f, kZone));
	leds.set_brightness(1, vu_brightness(mag_a, 1.0f, kZone));
	leds.set_brightness(2, vu_brightness(mag_a, 2.0f, kZone));

	leds.set_brightness(3, vu_brightness(mag_b, 0.0f, kZone));
	leds.set_brightness(4, vu_brightness(mag_b, 1.0f, kZone));
	leds.set_brightness(5, vu_brightness(mag_b, 2.0f, kZone));
}
