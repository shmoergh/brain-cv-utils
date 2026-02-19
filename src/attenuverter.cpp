#include "attenuverter.h"
#include <stdio.h>
#include <pico/time.h>

// C++11 ODR: constexpr static arrays need out-of-class definitions when address is taken
constexpr uint8_t Attenuverter::kLedsCh1[];
constexpr uint8_t Attenuverter::kLedsCh2[];


namespace {
int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
}

void Attenuverter::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
						  brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds) {
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
	uint16_t dac_ch1 = static_cast<uint16_t>(clamp16(out_ch1, 0, kDacMax));
	uint16_t dac_ch2 = static_cast<uint16_t>(clamp16(out_ch2, 0, kDacMax));

	// Write to DAC (SDK expects float volts: 0-4095 → 0.0-10.0V)
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA,
					   static_cast<float>(dac_ch1) * 10.0f / kDacMax);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB,
					   static_cast<float>(dac_ch2) * 10.0f / kDacMax);

	// VU meter LEDs: 3 per channel, bipolar display
	update_vu_leds(static_cast<int16_t>(dac_ch1) - kDacCenter, kLedsCh1, leds);
	update_vu_leds(static_cast<int16_t>(dac_ch2) - kDacCenter, kLedsCh2, leds);
}

void Attenuverter::update_vu_leds(int16_t dac_value, const uint8_t led_indices[3],
								  brain::ui::Leds& leds) {
	// Signal range ±2048, split into 3 zones of ~683 each
	static constexpr int16_t kZone = 683;

	int16_t mag = dac_value < 0 ? -dac_value : dac_value;

	uint8_t b0 = static_cast<uint8_t>(clamp16(mag * 255 / kZone, 0, 255));
	uint8_t b1 = static_cast<uint8_t>(clamp16((mag - kZone) * 255 / kZone, 0, 255));
	uint8_t b2 = static_cast<uint8_t>(clamp16((mag - 2 * kZone) * 255 / kZone, 0, 255));

	if (dac_value >= 0) {
		// Positive: fill left→right (0→1→2)
		leds.set_brightness(led_indices[0], b0);
		leds.set_brightness(led_indices[1], b1);
		leds.set_brightness(led_indices[2], b2);
	} else {
		// Negative: fill right→left (2→1→0)
		leds.set_brightness(led_indices[2], b0);
		leds.set_brightness(led_indices[1], b1);
		leds.set_brightness(led_indices[0], b2);
	}
}
