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

	// Pot 1/2: octave offset — map 0-255 to -4..+4 (9 steps)
	int8_t octave_ch1 = static_cast<int8_t>(pots.get(kPotOctaveCh1) * 9 / 256) - 4;
	int8_t octave_ch2 = static_cast<int8_t>(pots.get(kPotOctaveCh2) * 9 / 256) - 4;

	// Pot 3: fine tune bipolar
	const uint8_t fine_raw = pots.get(kPotFineTune);
	int32_t fine_tune_mv = 0;
	if (fine_raw > 128) {
		fine_tune_mv =
			(static_cast<int32_t>(fine_raw - 128) * kFineTuneMaxMillivolts + 63) / 127;
	} else if (fine_raw < 128) {
		fine_tune_mv =
			-((static_cast<int32_t>(128 - fine_raw) * kFineTuneMaxMillivolts + 64) / 128);
	}

	// Offsets in signal domain (-5V..+5V).
	const int32_t offset_ch1_mv =
		static_cast<int32_t>(octave_ch1) * kMillivoltsPerOctave + fine_tune_mv;
	const int32_t offset_ch2_mv =
		static_cast<int32_t>(octave_ch2) * kMillivoltsPerOctave + fine_tune_mv;

	// Read calibrated SDK input voltage directly in signal domain.
	const int32_t in_ch1_mv = cv_in.get_voltage_millivolts_channel_a();
	const int32_t in_ch2_mv = cv_in.get_voltage_millivolts_channel_b();

	// Add musical offsets and clamp to module signal range.
	const int32_t target_a_mv =
		clamp32(in_ch1_mv + offset_ch1_mv, kMinSignalMillivolts, kMaxSignalMillivolts);
	const int32_t target_b_mv =
		clamp32(in_ch2_mv + offset_ch2_mv, kMinSignalMillivolts, kMaxSignalMillivolts);

	const int32_t smooth_a_mv = smoother_ch1_.process(target_a_mv);
	const int32_t smooth_b_mv = smoother_ch2_.process(target_b_mv);

	const float out_a_voltage =
		static_cast<float>(smooth_a_mv + kCenterMillivolts) / 1000.0f;
	const float out_b_voltage =
		static_cast<float>(smooth_b_mv + kCenterMillivolts) / 1000.0f;
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelA, smooth_a_mv);
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelB, smooth_b_mv);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
}
