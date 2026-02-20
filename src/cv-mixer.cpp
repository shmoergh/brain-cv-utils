#include "cv-mixer.h"

// C++11 ODR: constexpr static arrays need out-of-class definitions
constexpr uint8_t CvMixer::kLedsCh1[];
constexpr uint8_t CvMixer::kLedsCh2[];

namespace {
float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void CvMixer::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
					  brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
					  bool allow_led_updates) {
	// Read input voltages (AC coupled)
	float in_a = cv_in.get_voltage_channel_a();
	float in_b = cv_in.get_voltage_channel_b();

	// Pot 1/2: input levels (0-255 → 0.0-1.0)
	float level_a = static_cast<float>(pots.get(kPotLevelA)) / 255.0f;
	float level_b = static_cast<float>(pots.get(kPotLevelB)) / 255.0f;

	// Pot 3: main output level (0-255 → 0.0-1.0)
	float main_level = static_cast<float>(pots.get(kPotMain)) / 255.0f;

	// Mix: scale each input by its level, sum, then apply main level
	float mix = (in_a * level_a + in_b * level_b) * main_level;

	// Mix is in bipolar signal domain (-5V..+5V). Convert to DAC voltage domain
	// (0V..10V where 5V represents 0V signal at the output stage).
	float signal = clampf(mix, kMinSignalVoltage, kMaxSignalVoltage);
	float out = signal + kCenterVoltage;

	// Output the same mix to both channels
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out);

	if (!allow_led_updates) {
		return;
	}

	// VU LEDs: left 3 for input A level, right 3 for input B level
	update_vu_leds(in_a * level_a * main_level, kLedsCh1, leds);
	update_vu_leds(in_b * level_b * main_level, kLedsCh2, leds);
}

void CvMixer::update_vu_leds(float voltage, const uint8_t led_indices[3],
							  brain::ui::Leds& leds) {
	// Map 0-10V across 3 LEDs (~3.3V per LED zone)
	static constexpr float kZone = 10.0f / 3.0f;

	float mag = voltage < 0.0f ? -voltage : voltage;

	uint8_t b0 = static_cast<uint8_t>(clampf(mag * 255.0f / kZone, 0.0f, 255.0f));
	uint8_t b1 = static_cast<uint8_t>(clampf((mag - kZone) * 255.0f / kZone, 0.0f, 255.0f));
	uint8_t b2 = static_cast<uint8_t>(clampf((mag - 2.0f * kZone) * 255.0f / kZone, 0.0f, 255.0f));

	leds.set_brightness(led_indices[0], b0);
	leds.set_brightness(led_indices[1], b1);
	leds.set_brightness(led_indices[2], b2);
}
