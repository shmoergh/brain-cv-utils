#include "precision-adder.h"

#include <cstring>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"

// C++11 ODR: constexpr static arrays need out-of-class definitions
constexpr uint8_t PrecisionAdder::kLedsCh1[];
constexpr uint8_t PrecisionAdder::kLedsCh2[];

namespace {
int32_t clamp32(int32_t v, int32_t lo, int32_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}
int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

constexpr uint32_t kCalibrationMagic = 0x5043414C;     // "PCAL"
constexpr uint16_t kCalibrationVersion = 1;
constexpr uint32_t kCalibrationFlashOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

struct CalibrationStorage {
	uint32_t magic;
	uint16_t version;
	int16_t trim_a;
	int16_t trim_b;
	uint16_t checksum;
	uint16_t reserved;
};

uint16_t calibration_checksum(const CalibrationStorage& data) {
	uint32_t sum = 0;
	sum += (data.magic & 0xFFFFu);
	sum += ((data.magic >> 16) & 0xFFFFu);
	sum += data.version;
	sum += static_cast<uint16_t>(data.trim_a);
	sum += static_cast<uint16_t>(data.trim_b);
	sum += data.reserved;
	return static_cast<uint16_t>(sum & 0xFFFFu);
}
}

void PrecisionAdder::init() {
	load_calibration_from_flash();
}

void PrecisionAdder::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
							brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
							bool button_a_pressed, bool button_b_pressed) {
	const bool released_a = button_a_prev_ && !button_a_pressed;
	const bool released_b = button_b_prev_ && !button_b_pressed;
	button_a_prev_ = button_a_pressed;
	button_b_prev_ = button_b_pressed;

	const bool calibrating_a = button_a_pressed && !button_b_pressed;
	const bool calibrating_b = button_b_pressed && !button_a_pressed;
	const bool calibration_mode = calibrating_a || calibrating_b;

	if (calibration_mode) {
		// Hold button A + turn POT1 to set ADC gain trim in a small range.
		const int32_t trim_span = static_cast<int32_t>(kAdcGainTrimMax - kAdcGainTrimMin);
		const int16_t trim_value = static_cast<int16_t>(
			kAdcGainTrimMin + (static_cast<int32_t>(pots.get(kPotOctaveCh1)) * trim_span + 127) / 255);
		if (calibrating_a) {
			if (adc_gain_trim_a_ != trim_value) {
				adc_gain_trim_a_ = trim_value;
				calibration_dirty_ = true;
			}
		} else {
			if (adc_gain_trim_b_ != trim_value) {
				adc_gain_trim_b_ = trim_value;
				calibration_dirty_ = true;
			}
		}
	}

	if (calibration_dirty_ && (released_a || released_b)) {
		save_calibration_to_flash();
		calibration_dirty_ = false;
	}

	// Pot 1/2: octave offset — map 0-255 to -4..+4 (9 steps)
	int8_t octave_ch1 = static_cast<int8_t>(pots.get(kPotOctaveCh1) * 9 / 256) - 4;
	int8_t octave_ch2 = static_cast<int8_t>(pots.get(kPotOctaveCh2) * 9 / 256) - 4;

	// Pot 3: fine tune bipolar mapping with center at 128:
	// down -> negative, center -> 0, up -> positive.
	const uint8_t fine_raw = pots.get(kPotFineTune);
	int16_t fine_tune = 0;
	if (fine_raw > 128) {
		fine_tune = static_cast<int16_t>(
			(static_cast<int32_t>(fine_raw - 128) * kFineTuneMax + 63) / 127);
	} else if (fine_raw < 128) {
		fine_tune = static_cast<int16_t>(
			-((static_cast<int32_t>(128 - fine_raw) * kFineTuneMax + 64) / 128));
	}

	// Offsets in DAC units (1 octave = 1V = kDacPerVolt DAC units).
	// During calibration mode, ignore octave offsets but keep fine tune active.
	int16_t offset_ch1 = fine_tune;
	int16_t offset_ch2 = fine_tune;
	if (!calibration_mode) {
		offset_ch1 = static_cast<int16_t>(octave_ch1) * kDacPerVolt + fine_tune;
		offset_ch2 = static_cast<int16_t>(octave_ch2) * kDacPerVolt + fine_tune;
	}

	// Read raw ADC and map from ADC domain to DAC domain
	// ADC [kAdcAtMinus5V..kAdcAtPlus5V] → DAC [0..4095] (both represent -5V to +5V)
	int32_t dac_ch1 = static_cast<int32_t>(cv_in.get_raw_channel_a() - kAdcAtMinus5V) *
					  kDacMax / kAdcSpan;
	int32_t dac_ch2 = static_cast<int32_t>(cv_in.get_raw_channel_b() - kAdcAtMinus5V) *
					  kDacMax / kAdcSpan;

	// Apply ADC gain calibration before offsets.
	dac_ch1 = dac_ch1 * (kCalibScale + adc_gain_trim_a_) / kCalibScale;
	dac_ch2 = dac_ch2 * (kCalibScale + adc_gain_trim_b_) / kCalibScale;

	// Add offset and clamp to DAC range
	dac_ch1 = clamp32(dac_ch1 + offset_ch1, 0, kDacMax);
	dac_ch2 = clamp32(dac_ch2 + offset_ch2, 0, kDacMax);

	// Write to DAC (only float conversion, required by SDK API)
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA,
					   static_cast<float>(dac_ch1) * 10.0f / kDacMax);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB,
					   static_cast<float>(dac_ch2) * 10.0f / kDacMax);

	// LED feedback: 3 LEDs per channel showing octave offset
	if (calibration_mode) {
		update_calibration_leds(calibrating_a ? adc_gain_trim_a_ : adc_gain_trim_b_, leds);
	} else {
		update_offset_leds(octave_ch1, kLedsCh1, leds);
		update_offset_leds(octave_ch2, kLedsCh2, leds);
	}
}

void PrecisionAdder::update_offset_leds(int8_t octave, const uint8_t led_indices[3],
										 brain::ui::Leds& leds) {
	// Show octave offset using 3 LEDs as a bar display
	// -4..-1: fill from right (LED 2→1→0), brightness by magnitude
	// 0: all off
	// +1..+4: fill from left (LED 0→1→2), brightness by magnitude
	int8_t mag = octave < 0 ? -octave : octave;

	// Each LED represents ~1.3 octaves, scale brightness
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

void PrecisionAdder::update_calibration_leds(int16_t trim_value, brain::ui::Leds& leds) {
	// 6 LEDs as a center-out meter:
	// negative: 2,1,0 | positive: 3,4,5
	for (uint8_t i = 0; i < 6; i++) {
		leds.set_brightness(i, 0);
	}

	const int32_t abs_trim = trim_value < 0 ? -trim_value : trim_value;
	const int32_t max_meter = 3 * 255;
	const int32_t meter = abs_trim * max_meter / kAdcGainTrimMax;

	const uint8_t b0 = static_cast<uint8_t>(clamp32(meter, 0, 255));
	const uint8_t b1 = static_cast<uint8_t>(clamp32(meter - 255, 0, 255));
	const uint8_t b2 = static_cast<uint8_t>(clamp32(meter - 510, 0, 255));

	if (trim_value >= 0) {
		leds.set_brightness(3, b0);
		leds.set_brightness(4, b1);
		leds.set_brightness(5, b2);
	} else {
		leds.set_brightness(2, b0);
		leds.set_brightness(1, b1);
		leds.set_brightness(0, b2);
	}
}

void PrecisionAdder::load_calibration_from_flash() {
	const auto* data =
		reinterpret_cast<const CalibrationStorage*>(XIP_BASE + kCalibrationFlashOffset);

	if (data->magic != kCalibrationMagic || data->version != kCalibrationVersion) {
		adc_gain_trim_a_ = 0;
		adc_gain_trim_b_ = 0;
		return;
	}
	if (data->checksum != calibration_checksum(*data)) {
		adc_gain_trim_a_ = 0;
		adc_gain_trim_b_ = 0;
		return;
	}

	adc_gain_trim_a_ = clamp16(data->trim_a, kAdcGainTrimMin, kAdcGainTrimMax);
	adc_gain_trim_b_ = clamp16(data->trim_b, kAdcGainTrimMin, kAdcGainTrimMax);
}

void PrecisionAdder::save_calibration_to_flash() {
	CalibrationStorage data;
	data.magic = kCalibrationMagic;
	data.version = kCalibrationVersion;
	data.trim_a = adc_gain_trim_a_;
	data.trim_b = adc_gain_trim_b_;
	data.reserved = 0xFFFFu;
	data.checksum = calibration_checksum(data);

	uint8_t page_buffer[FLASH_PAGE_SIZE];
	memset(page_buffer, 0xFF, sizeof(page_buffer));
	memcpy(page_buffer, &data, sizeof(data));

	uint32_t interrupts = save_and_disable_interrupts();
	flash_range_erase(kCalibrationFlashOffset, FLASH_SECTOR_SIZE);
	flash_range_program(kCalibrationFlashOffset, page_buffer, FLASH_PAGE_SIZE);
	restore_interrupts(interrupts);
}
