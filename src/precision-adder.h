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

	static constexpr int32_t kMinSignalMillivolts = -5000;
	static constexpr int32_t kMaxSignalMillivolts = 5000;
	static constexpr int32_t kCenterMillivolts = 5000;
	static constexpr int32_t kMillivoltsPerOctave = 1000;
	// Fine tune: ±5 semitones ~= ±0.417V
	static constexpr int32_t kFineTuneMaxMillivolts = 417;

	// Anti-jitter smoothing (small deadband, no extra lag by default).
	static constexpr int32_t kSmoothingDeadbandMv = 7;
	static constexpr uint16_t kSmoothingAlphaQ15 = 16384;	// 0.5

	VoltageSmoother smoother_ch1_{kSmoothingDeadbandMv, kSmoothingAlphaQ15};
	VoltageSmoother smoother_ch2_{kSmoothingDeadbandMv, kSmoothingAlphaQ15};
};

#endif  // PRECISION_ADDER_H_
