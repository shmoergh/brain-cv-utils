#ifndef PRECISION_ADDER_H_
#define PRECISION_ADDER_H_

#include <cstdint>

#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class PrecisionAdder {
public:
	void init();
	void update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
				brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
				bool button_a_pressed, bool button_b_pressed);

private:
	static constexpr uint8_t kPotOctaveCh1 = 0;
	static constexpr uint8_t kPotOctaveCh2 = 1;
	static constexpr uint8_t kPotFineTune = 2;

	static constexpr uint16_t kDacMax = 4095;

	// 1V/oct: 4095 DAC units / 10V = ~410 DAC units per volt
	static constexpr int16_t kDacPerVolt = 410;

	// Fine tune: ±5 semitone = ±1/12 V ≈ ±34 DAC units
	static constexpr int16_t kFineTuneMax = 34 * 5;

	// ADC raw values at calibration points, derived from SDK constants:
	// kAdcAtMinus5V = 0.240V / 3.3V * 4095 ≈ 298
	// kAdcAtPlus5V  = 3.000V / 3.3V * 4095 ≈ 3723
	static constexpr uint16_t kAdcAtMinus5V = 298;
	static constexpr uint16_t kAdcAtPlus5V = 3723;
	static constexpr uint16_t kAdcSpan = kAdcAtPlus5V - kAdcAtMinus5V;

	// LEDs: 3 per channel for offset display
	static constexpr uint8_t kLedsCh1[3] = {0, 1, 2};
	static constexpr uint8_t kLedsCh2[3] = {3, 4, 5};

	// ADC gain calibration multiplier:
	// corrected = raw_mapped * (kCalibScale + adc_gain_trim_) / kCalibScale
	// adc_gain_trim_ is set by holding button A and turning POT1.
	static constexpr int32_t kCalibScale = 10000;
	static constexpr int16_t kAdcGainTrimMin = -300;  // -3.00%
	static constexpr int16_t kAdcGainTrimMax = 300;   // +3.00%

	int16_t adc_gain_trim_a_ = 0;
	int16_t adc_gain_trim_b_ = 0;
	bool calibration_dirty_ = false;
	bool button_a_prev_ = false;
	bool button_b_prev_ = false;

	static void update_offset_leds(int8_t octave, const uint8_t led_indices[3],
									brain::ui::Leds& leds);
	static void update_calibration_leds(int16_t trim_value, brain::ui::Leds& leds);

	void load_calibration_from_flash();
	void save_calibration_to_flash();
};

#endif  // PRECISION_ADDER_H_
