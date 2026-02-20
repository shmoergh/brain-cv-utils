#include "cv-mixer.h"

namespace {
float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void CvMixer::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
					  brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
					  LedController& led_controller) {
	float in_a = cv_in.get_voltage_channel_a();
	float in_b = cv_in.get_voltage_channel_b();

	float level_a = static_cast<float>(pots.get(kPotLevelA)) / 255.0f;
	float level_b = static_cast<float>(pots.get(kPotLevelB)) / 255.0f;
	float main_level = static_cast<float>(pots.get(kPotMain)) / 255.0f;

	float mix = (in_a * level_a + in_b * level_b) * main_level;
	float signal = clampf(mix, kMinSignalVoltage, kMaxSignalVoltage);
	float out = signal + kCenterVoltage;

	const float out_a_voltage = out;
	const float out_b_voltage = out;
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_a_voltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_b_voltage);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
}
