#ifndef PRECISION_ADDER_H_
#define PRECISION_ADDER_H_

#include <cstdint>

#include "brain/include/inputs.h"
#include "brain/include/leds.h"
#include "brain/include/outputs.h"
#include "brain/include/pots.h"
#include "led-controller.h"
#include "voltage-smoother.h"

class PrecisionAdder {
public:
	void update(Pots& pots, Inputs& cv_in, Outputs& cv_out,
				bool button_b_pressed, Leds& leds, LedController& led_controller);

private:
	static constexpr uint8_t kPotOctaveCh1 = 0;
	static constexpr uint8_t kPotOctaveCh2 = 1;
	static constexpr uint8_t kPotFineTune = 2;

	// Output runs in unipolar 0..10V (CvUtils::set_mode configures this for the
	// Precision Adder mode). All knobs are positive-only so that "all pots at
	// minimum" produces 0V at OUT.
	static constexpr int32_t kMinDacMillivolts = 0;
	static constexpr int32_t kMaxDacMillivolts = 10000;
	static constexpr int32_t kMillivoltsPerOctave = 1000;
	static constexpr uint8_t kOctaveStepCount = 9;  // 0V, 1V, …, 8V
	// Fine tune: 0..+1V additive offset.
	static constexpr int32_t kFineTuneMaxMillivolts = 1000;

	// Anti-jitter smoothing (small deadband, no extra lag by default).
	static constexpr int32_t kSmoothingDeadbandMv = 7;
	static constexpr uint16_t kSmoothingAlphaQ15 = 16384;	// 0.5

	VoltageSmoother smoother_ch1_{kSmoothingDeadbandMv, kSmoothingAlphaQ15};
	VoltageSmoother smoother_ch2_{kSmoothingDeadbandMv, kSmoothingAlphaQ15};
};

#endif  // PRECISION_ADDER_H_
