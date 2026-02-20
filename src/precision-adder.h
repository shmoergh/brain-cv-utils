#ifndef PRECISION_ADDER_H_
#define PRECISION_ADDER_H_

#include <cstdint>

#include "calibration.h"
#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class PrecisionAdder {
public:
	void update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
				brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
				Calibration& calibration, bool button_b_pressed,
				bool allow_led_updates = true);

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

	// LEDs: 3 per channel for offset display
	static constexpr uint8_t kLedsCh1[3] = {0, 1, 2};
	static constexpr uint8_t kLedsCh2[3] = {3, 4, 5};

	static void update_offset_leds(int8_t octave, const uint8_t led_indices[3],
									brain::ui::Leds& leds);
};

#endif  // PRECISION_ADDER_H_
