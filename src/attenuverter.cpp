#include "attenuverter.h"

namespace {
int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void Attenuverter::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
						  brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
						  LedController& led_controller) {
	// Pots: 0-255, ADC/DAC: 0-4095

	// Attenuation: pot 0 → -256, pot 128 → 0, pot 255 → +254
	int16_t atten_ch1 = (static_cast<int16_t>(pots.get(kPotAttenCh1)) - 128) * 2;
	int16_t atten_ch2 = (static_cast<int16_t>(pots.get(kPotAttenCh2)) - 128) * 2;

	// DC offset as DAC units: pot 0 → -2048, pot 128 → 0, pot 255 → +2047
	int16_t dc_offset = (static_cast<int16_t>(pots.get(kPotDcOffset)) - 128) * 16;

	// CV input as signed: raw 0-4095 → signed -2048 to +2047
	int16_t in_ch1 = static_cast<int16_t>(cv_in.get_raw_channel_a()) - kDacCenter;
	int16_t in_ch2 = static_cast<int16_t>(cv_in.get_raw_channel_b()) - kDacCenter;

	// Attenuate and shift to unsigned DAC range
	// (in * atten) / 256 keeps result in ~12-bit range, then add center + offset
	int16_t out_ch1 = static_cast<int16_t>((in_ch1 * atten_ch1) / 256) + kDacCenter + dc_offset;
	int16_t out_ch2 = static_cast<int16_t>((in_ch2 * atten_ch2) / 256) + kDacCenter + dc_offset;

	// Clamp to DAC range
	const uint16_t dac_ch1 = static_cast<uint16_t>(clamp16(out_ch1, 0, kDacMax));
	const uint16_t dac_ch2 = static_cast<uint16_t>(clamp16(out_ch2, 0, kDacMax));

	const float out_a_voltage = static_cast<float>(dac_ch1) * 10.0f / kDacMax;
	const float out_b_voltage = static_cast<float>(dac_ch2) * 10.0f / kDacMax;
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_a_voltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_b_voltage);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);
}
