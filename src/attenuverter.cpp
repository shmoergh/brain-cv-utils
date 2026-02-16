#include "attenuverter.h"

#include <algorithm>

void Attenuverter::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
						  brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds) {
	// Read pots (0-255)
	int32_t pot_ch1 = pots.get(kPotAttenCh1);
	int32_t pot_ch2 = pots.get(kPotAttenCh2);
	int32_t pot_offset = pots.get(kPotDcOffset);

	// Attenuation factor as signed fixed-point: -256 to +256 (center = 0)
	// pot 0 → -256, pot 128 → 0, pot 255 → +256
	int32_t atten_ch1 = (pot_ch1 - 128) * 2;
	int32_t atten_ch2 = (pot_ch2 - 128) * 2;

	// DC offset in millivolts: -5000 to +5000
	int32_t dc_offset_mv = ((pot_offset - 128) * kMaxDcOffsetMv) / 128;

	// Read CV inputs as raw ADC (0-4095)
	int32_t raw_ch1 = cv_in.get_raw_channel_a();
	int32_t raw_ch2 = cv_in.get_raw_channel_b();

	// Convert raw ADC to millivolts (-5000 to +5000)
	// ADC 0 → -5000mV, ADC 2048 → 0mV, ADC 4095 → +5000mV
	int32_t in_mv_ch1 = ((raw_ch1 - 2048) * 5000) / 2048;
	int32_t in_mv_ch2 = ((raw_ch2 - 2048) * 5000) / 2048;

	// Apply attenuation: (input_mv * atten) / 256 + offset + bias
	int32_t out_mv_ch1 = (in_mv_ch1 * atten_ch1) / 256 + dc_offset_mv + kBiasShiftMv;
	int32_t out_mv_ch2 = (in_mv_ch2 * atten_ch2) / 256 + dc_offset_mv + kBiasShiftMv;

	// Clamp to output range (0-10000 mV)
	out_mv_ch1 = std::clamp(out_mv_ch1, static_cast<int32_t>(0), kMaxOutputMv);
	out_mv_ch2 = std::clamp(out_mv_ch2, static_cast<int32_t>(0), kMaxOutputMv);

	// Convert millivolts to DAC value (0-4095)
	uint16_t dac_ch1 = static_cast<uint16_t>((out_mv_ch1 * 4095) / kMaxOutputMv);
	uint16_t dac_ch2 = static_cast<uint16_t>((out_mv_ch2 * 4095) / kMaxOutputMv);

	// Write to DAC — use set_voltage with mV→V conversion (SDK expects float here)
	// TODO: If SDK adds integer DAC write, switch to that
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA,
					   static_cast<float>(out_mv_ch1) / 1000.0f);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB,
					   static_cast<float>(out_mv_ch2) / 1000.0f);

	// LED feedback: brightness proportional to output (0-255)
	uint8_t led_ch1 = static_cast<uint8_t>((out_mv_ch1 * 255) / kMaxOutputMv);
	uint8_t led_ch2 = static_cast<uint8_t>((out_mv_ch2 * 255) / kMaxOutputMv);
	leds.set_brightness(kLedCh1, led_ch1);
	leds.set_brightness(kLedCh2, led_ch2);
}
