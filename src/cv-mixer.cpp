#include "cv-mixer.h"

namespace {
float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void CvMixer::update(Pots& pots, Inputs& cv_in, Outputs& cv_out, Leds& leds,
					  LedController& led_controller) {
	float in_a = static_cast<float>(cv_in.get_voltage_millivolts_channel_a()) / 1000.0f;
	float in_b = static_cast<float>(cv_in.get_voltage_millivolts_channel_b()) / 1000.0f;

	float level_a = static_cast<float>(pots.get(kPotLevelA)) / 255.0f;
	float level_b = static_cast<float>(pots.get(kPotLevelB)) / 255.0f;
	float main_level = static_cast<float>(pots.get(kPotMain)) / 255.0f;

	float mix = (in_a * level_a + in_b * level_b) * main_level;
	float signal = clampf(mix, kMinSignalVoltage, kMaxSignalVoltage);
	float out = signal + kCenterVoltage;

	const float out_a_voltage = out;
	const float out_b_voltage = out;
	const int32_t out_mv = static_cast<int32_t>(out * 1000.0f + 0.5f);
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelA, out_mv - 5000);
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelB, out_mv - 5000);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
}
