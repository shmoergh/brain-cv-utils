#ifndef SLEW_LIMITER_H_
#define SLEW_LIMITER_H_

#include <cstdint>

#include "calibration.h"
#include "led-controller.h"
#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class SlewLimiter {
public:
	SlewLimiter();

	void update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
				brain::io::AudioCvOut& cv_out,
				Calibration& calibration, bool button_b_pressed,
				brain::ui::Leds& leds, LedController& led_controller);

private:
	static constexpr uint8_t kPotRise = 0;
	static constexpr uint8_t kPotFall = 1;
	static constexpr uint8_t kPotShape = 2;
	static constexpr float kMaxVoltage = 10.0f;
	static constexpr uint32_t kMaxSlewUs = 2000000;  // ~2 seconds

	static float slew_channel(float input, float current, float rise_coeff,
							  float fall_coeff, float shape);

	// State
	float current_ch1_;
	float current_ch2_;
	uint32_t last_time_us_;
	bool linked_;
	bool button_b_prev_;
};

#endif  // SLEW_LIMITER_H_
