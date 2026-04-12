#ifndef PRECISION_ADDER_H_
#define PRECISION_ADDER_H_

#include <cstdint>

#include "brain/include/inputs.h"
#include "brain/include/leds.h"
#include "brain/include/outputs.h"
#include "brain/include/pots.h"
#include "calibration.h"
#include "led-controller.h"
#include "voltage-smoother.h"

class PrecisionAdder {
public:
	void update(Pots& pots, Inputs& cv_in, Outputs& cv_out, Calibration& calibration,
				bool button_b_pressed, Leds& leds, LedController& led_controller);

private:
	static constexpr uint8_t kPotOctaveCh1 = 0;
	static constexpr uint8_t kPotOctaveCh2 = 1;
	static constexpr uint8_t kPotFineTune = 2;

	static constexpr uint16_t kDacMax = 4095;

	// 1V/oct: 4095 DAC units / 10V = ~410 DAC units per volt
	static constexpr int16_t kDacPerVolt = 410;

	// Fine tune: ±5 semitones ≈ ±170 DAC units
	static constexpr int16_t kFineTuneMax = 34 * 5;

	// ADC raw values at calibration points
	static constexpr uint16_t kAdcAtMinus5V = 298;
	static constexpr uint16_t kAdcAtPlus5V = 3723;
	static constexpr uint16_t kAdcSpan = kAdcAtPlus5V - kAdcAtMinus5V;

	// Anti-jitter smoothing (small deadband, no extra lag by default).
	static constexpr int32_t kSmoothingDeadbandMv = 7;
	static constexpr uint16_t kSmoothingAlphaQ15 = 16384;	// 0.5

	VoltageSmoother smoother_ch1_{kSmoothingDeadbandMv, kSmoothingAlphaQ15};
	VoltageSmoother smoother_ch2_{kSmoothingDeadbandMv, kSmoothingAlphaQ15};
};

#endif  // PRECISION_ADDER_H_
