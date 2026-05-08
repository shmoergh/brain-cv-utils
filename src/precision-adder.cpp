#include "precision-adder.h"

namespace {
int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void PrecisionAdder::update(Pots& pots, Inputs& cv_in, Outputs& cv_out,
							bool button_b_pressed,
							Leds& leds, LedController& led_controller) {
	(void)button_b_pressed;

	// Pot 1/2: octave offset — map 0..255 to 0..+8 V (9 detents).
	const int32_t octave_ch1 =
		static_cast<int32_t>(pots.get(kPotOctaveCh1)) * kOctaveStepCount / 256;
	const int32_t octave_ch2 =
		static_cast<int32_t>(pots.get(kPotOctaveCh2)) * kOctaveStepCount / 256;

	// Pot 3: fine tune — map 0..255 to 0..+kFineTuneMaxMillivolts (positive-only).
	const uint8_t fine_raw = pots.get(kPotFineTune);
	const int32_t fine_tune_mv =
		(static_cast<int32_t>(fine_raw) * kFineTuneMaxMillivolts + 127) / 255;

	const int32_t offset_ch1_mv = octave_ch1 * kMillivoltsPerOctave + fine_tune_mv;
	const int32_t offset_ch2_mv = octave_ch2 * kMillivoltsPerOctave + fine_tune_mv;

	const int32_t in_ch1_mv = cv_in.get_voltage_millivolts_channel_a();
	const int32_t in_ch2_mv = cv_in.get_voltage_millivolts_channel_b();

	// Sum and clamp to the unipolar DAC output range (0..10V). Negative input
	// (which the SDK still reports as bipolar mV) is folded against the
	// positive knob offset and clamped to 0V if it would go below.
	const int32_t target_a_mv =
		clamp32(in_ch1_mv + offset_ch1_mv, kMinDacMillivolts, kMaxDacMillivolts);
	const int32_t target_b_mv =
		clamp32(in_ch2_mv + offset_ch2_mv, kMinDacMillivolts, kMaxDacMillivolts);

	const int32_t smooth_a_mv = smoother_ch1_.process(target_a_mv);
	const int32_t smooth_b_mv = smoother_ch2_.process(target_b_mv);

	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelA, smooth_a_mv);
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelB, smooth_b_mv);

	const float out_a_voltage = static_cast<float>(smooth_a_mv) / 1000.0f;
	const float out_b_voltage = static_cast<float>(smooth_b_mv) / 1000.0f;
	led_controller.render_output_vu_unipolar(leds, out_a_voltage, out_b_voltage);
}
