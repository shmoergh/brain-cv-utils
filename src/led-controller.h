#ifndef LED_CONTROLLER_H_
#define LED_CONTROLLER_H_

#include <cstdint>

#include "brain-ui/leds.h"

class LedController {
public:
	void start_mode_change(uint32_t now_us);
	bool is_mode_override_active(uint32_t now_us) const;
	void render_mode_change(brain::ui::Leds& leds, uint8_t mode_index,
							uint8_t num_modes, uint32_t now_us) const;
	void render_output_vu(brain::ui::Leds& leds, float out_a_voltage,
						  float out_b_voltage) const;

private:
	static constexpr uint32_t kModeLedBlinkHalfPeriodUs = 100000;  // 100ms
	static constexpr uint32_t kModeLedBlinkCount = 3;
	static constexpr uint32_t kModeLedHoldUs =
		kModeLedBlinkHalfPeriodUs * 2 * kModeLedBlinkCount;

	uint32_t mode_led_override_started_us_ = 0;
	uint32_t mode_led_override_until_us_ = 0;
};

#endif  // LED_CONTROLLER_H_
