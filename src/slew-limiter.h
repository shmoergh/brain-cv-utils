#ifndef SLEW_LIMITER_H_
#define SLEW_LIMITER_H_

#include <cstdint>

#include "calibration.h"
#include "led-controller.h"
#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"
#include "voltage-smoother.h"

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
	static constexpr int32_t kMinSignalMillivolts = -5000;
	static constexpr int32_t kMaxSignalMillivolts = 5000;
	static constexpr int32_t kCenterMillivolts = 5000;
	static constexpr int32_t kMaxMillivolts = 10000;
	static constexpr uint32_t kMaxSlewUs = 2000000;  // ~2 seconds
	static constexpr int32_t kOutputDeadbandMv = 5;
	static constexpr uint16_t kOutputSmoothingAlphaQ15 = 8192;	// 0.25

	static int32_t slew_channel_mv(int32_t input_mv, int32_t current_mv,
								   uint16_t rise_coeff_q15,
								   uint16_t fall_coeff_q15,
								   uint16_t shape_q15);

	// State
	int32_t current_ch1_mv_;
	int32_t current_ch2_mv_;
	VoltageSmoother output_smoother_ch1_;
	VoltageSmoother output_smoother_ch2_;
	uint32_t last_time_us_;
	bool linked_;
	bool button_b_prev_;
};

#endif  // SLEW_LIMITER_H_
