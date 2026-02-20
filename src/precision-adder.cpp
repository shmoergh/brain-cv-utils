#include "precision-adder.h"

// C++11 ODR
constexpr uint8_t PrecisionAdder::kLedsCh1[];
constexpr uint8_t PrecisionAdder::kLedsCh2[];

namespace {
int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void PrecisionAdder::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
							brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
							Calibration& calibration, bool button_b_pressed,
							bool allow_led_updates) {
	// Pot 1/2: octave offset — map 0-255 to -4..+4 (9 steps)
	int8_t octave_ch1 = static_cast<int8_t>(pots.get(kPotOctaveCh1) * 9 / 256) - 4;
	int8_t octave_ch2 = static_cast<int8_t>(pots.get(kPotOctaveCh2) * 9 / 256) - 4;

	// Pot 3: fine tune bipolar
	const uint8_t fine_raw = pots.get(kPotFineTune);
	int16_t fine_tune = 0;
	if (fine_raw > 128) {
		fine_tune = static_cast<int16_t>(
			(static_cast<int32_t>(fine_raw - 128) * kFineTuneMax + 63) / 127);
	} else if (fine_raw < 128) {
		fine_tune = static_cast<int16_t>(
			-((static_cast<int32_t>(128 - fine_raw) * kFineTuneMax + 64) / 128));
	}

	// Offsets in DAC units
	int16_t offset_ch1 = static_cast<int16_t>(octave_ch1) * kDacPerVolt + fine_tune;
	int16_t offset_ch2 = static_cast<int16_t>(octave_ch2) * kDacPerVolt + fine_tune;

	// Read raw ADC and map to DAC domain
	int32_t dac_ch1 = static_cast<int32_t>(cv_in.get_raw_channel_a() - kAdcAtMinus5V) *
					  kDacMax / kAdcSpan;
	int32_t dac_ch2 = static_cast<int32_t>(cv_in.get_raw_channel_b() - kAdcAtMinus5V) *
					  kDacMax / kAdcSpan;

	// Apply calibration: gain trim + offset trim
	dac_ch1 = dac_ch1 * (Calibration::kCalibScale + calibration.gain_trim_a()) / Calibration::kCalibScale;
	dac_ch2 = dac_ch2 * (Calibration::kCalibScale + calibration.gain_trim_b()) / Calibration::kCalibScale;
	dac_ch1 += calibration.offset_trim_a();
	dac_ch2 += calibration.offset_trim_b();

	// Add offset and clamp
	dac_ch1 = clamp32(dac_ch1 + offset_ch1, 0, kDacMax);
	dac_ch2 = clamp32(dac_ch2 + offset_ch2, 0, kDacMax);

	// Write to DAC
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA,
					   static_cast<float>(dac_ch1) * 10.0f / kDacMax);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB,
					   static_cast<float>(dac_ch2) * 10.0f / kDacMax);

	if (!allow_led_updates) {
		return;
	}

	// LED feedback
	update_offset_leds(octave_ch1, kLedsCh1, leds);
	update_offset_leds(octave_ch2, kLedsCh2, leds);
}

void PrecisionAdder::update_offset_leds(int8_t octave, const uint8_t led_indices[3],
										 brain::ui::Leds& leds) {
	int8_t mag = octave < 0 ? -octave : octave;

	uint8_t b0 = static_cast<uint8_t>(clamp16(mag * 85, 0, 255));
	uint8_t b1 = static_cast<uint8_t>(clamp16((mag - 1) * 85, 0, 255));
	uint8_t b2 = static_cast<uint8_t>(clamp16((mag - 3) * 85, 0, 255));

	if (octave >= 0) {
		leds.set_brightness(led_indices[0], b0);
		leds.set_brightness(led_indices[1], b1);
		leds.set_brightness(led_indices[2], b2);
	} else {
		leds.set_brightness(led_indices[2], b0);
		leds.set_brightness(led_indices[1], b1);
		leds.set_brightness(led_indices[0], b2);
	}
}
