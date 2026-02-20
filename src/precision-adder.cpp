#include "precision-adder.h"

namespace {
int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void PrecisionAdder::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
							brain::io::AudioCvOut& cv_out,
							Calibration& calibration, bool button_b_pressed,
							brain::ui::Leds& leds, LedController& led_controller) {
	(void)button_b_pressed;
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

	const float out_a_voltage = static_cast<float>(dac_ch1) * 10.0f / kDacMax;
	const float out_b_voltage = static_cast<float>(dac_ch2) * 10.0f / kDacMax;
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_a_voltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_b_voltage);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
}
