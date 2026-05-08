#ifndef SLEW_LIMITER_H_
#define SLEW_LIMITER_H_

#include <cstdint>

#include "brain/include/inputs.h"
#include "brain/include/leds.h"
#include "brain/include/outputs.h"
#include "brain/include/pots.h"
#include "led-controller.h"
#include "voltage-smoother.h"

class SlewLimiter {
public:
	SlewLimiter();

	void update(Pots& pots, Inputs& cv_in, Outputs& cv_out,
				bool button_b_pressed, Leds& leds, LedController& led_controller);

private:
	static constexpr uint8_t kPotRise = 0;
	static constexpr uint8_t kPotFall = 1;
	static constexpr uint8_t kPotOffset = 2;
	static constexpr int32_t kMaxMillivolts = 10000;
	// Pot 3 maps 0..255 to a 0..+1V additive fine-tune offset on the slew target.
	static constexpr int32_t kOffsetMaxMillivolts = 1000;
	static constexpr uint32_t kMaxSlewUs = 2000000;  // ~2 seconds
	static constexpr int32_t kOutputDeadbandMv = 5;
	static constexpr uint16_t kOutputSmoothingAlphaQ15 = 8192;	// 0.25
	// Slew step is throttled to this period so the per-tick coefficient does not
	// underflow to 0 at the SDK 2.1 main-loop rate (continuous-DMA ADC makes the
	// loop run at tens of kHz). 1 ms gives coeff_q15 >= ~16 at the slowest pot
	// setting (kMaxSlewUs), well above the 32768/kMaxMillivolts ≈ 3 truncation
	// threshold of the linear branch.
	static constexpr uint32_t kSlewStepPeriodUs = 1000;

	static int32_t slew_channel_mv(int32_t input_mv, int32_t current_mv,
								   uint16_t rise_coeff_q15,
								   uint16_t fall_coeff_q15);

	// State
	int32_t current_ch1_mv_;
	int32_t current_ch2_mv_;
	VoltageSmoother output_smoother_ch1_;
	VoltageSmoother output_smoother_ch2_;
	uint32_t last_time_us_;
	uint32_t slew_dt_accum_us_;
	bool linked_;
	bool button_b_prev_;
};

#endif  // SLEW_LIMITER_H_
