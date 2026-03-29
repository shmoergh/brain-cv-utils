#include "calibration.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "brain-storage/storage.h"
#include "pico/time.h"

namespace {

int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

constexpr uint32_t kMagic = 0x544C4143;	 // "CALT"
constexpr uint16_t kVersion = 1;

// Blink period in microseconds (500ms on, 500ms off)
constexpr uint32_t kBlinkPeriodUs = 500000;

struct CalibrationAppBlobV1 {
	uint32_t magic;
	uint16_t version;
	int16_t gain_trim_a;
	int16_t gain_trim_b;
	int16_t offset_trim_a;
	int16_t offset_trim_b;
};

int16_t pot_to_gain_trim(uint8_t pot_value, int16_t min_val, int16_t max_val) {
	int32_t span = static_cast<int32_t>(max_val - min_val);
	return static_cast<int16_t>(min_val + (static_cast<int32_t>(pot_value) * span + 127) / 255);
}

int16_t pot_to_offset_trim(uint8_t pot_value, int16_t min_val, int16_t max_val) {
	int32_t span = static_cast<int32_t>(max_val - min_val);
	return static_cast<int16_t>(min_val + (static_cast<int32_t>(pot_value) * span + 127) / 255);
}

}  // namespace

Calibration::Calibration()
	: gain_trim_a_(0),
	  gain_trim_b_(0),
	  offset_trim_a_(0),
	  offset_trim_b_(0) {}

void Calibration::init() {
	load_from_app_blob();
}

void Calibration::update_from_pots(brain::ui::Pots& pots,
								   bool button_a_held, bool button_b_held) {
	if (button_a_held) {
		// Hold A + Pot 3 -> offset A
		offset_trim_a_ = pot_to_offset_trim(pots.get(2), kOffsetTrimMin, kOffsetTrimMax);
	} else if (button_b_held) {
		// Hold B + Pot 3 -> offset B
		offset_trim_b_ = pot_to_offset_trim(pots.get(2), kOffsetTrimMin, kOffsetTrimMax);
	} else {
		// No button held: Pot 1 = scale A, Pot 2 = scale B
		gain_trim_a_ = pot_to_gain_trim(pots.get(0), kGainTrimMin, kGainTrimMax);
		gain_trim_b_ = pot_to_gain_trim(pots.get(1), kGainTrimMin, kGainTrimMax);
	}
}

void Calibration::save() {
	save_to_app_blob();
}

void Calibration::process_passthrough(brain::io::AudioCvIn& cv_in,
									  brain::io::AudioCvOut& cv_out) const {
	const float in_a = cv_in.get_voltage_channel_a();
	const float in_b = cv_in.get_voltage_channel_b();

	constexpr float kDacMax = 4095.0f;
	constexpr float kOffsetTrimToVolts = 10.0f / kDacMax;

	// Direct voltage passthrough with app-level trim correction.
	float out_a = in_a * static_cast<float>(kCalibScale + gain_trim_a_) /
				  static_cast<float>(kCalibScale);
	float out_b = in_b * static_cast<float>(kCalibScale + gain_trim_b_) /
				  static_cast<float>(kCalibScale);
	out_a += static_cast<float>(offset_trim_a_) * kOffsetTrimToVolts;
	out_b += static_cast<float>(offset_trim_b_) * kOffsetTrimToVolts;

	out_a = out_a < 0.0f ? 0.0f : (out_a > 10.0f ? 10.0f : out_a);
	out_b = out_b < 0.0f ? 0.0f : (out_b > 10.0f ? 10.0f : out_b);

	cv_out.set_voltage_calibrated(brain::io::AudioCvOutChannel::kChannelA, out_a);
	cv_out.set_voltage_calibrated(brain::io::AudioCvOutChannel::kChannelB, out_b);
}

void Calibration::update_leds(brain::ui::Leds& leds) {
	// Blink all LEDs
	uint32_t now = time_us_32();
	uint32_t phase = (now / kBlinkPeriodUs) % 2;

	for (uint8_t i = 0; i < 6; i++) {
		if (phase == 0) {
			leds.on(i);
		} else {
			leds.off(i);
		}
	}
}

void Calibration::load_from_app_blob() {
	CalibrationAppBlobV1 blob{};
	size_t actual_size = 0;
	const brain::storage::StorageStatus status =
		brain::storage::read_app_blob(&blob, sizeof(blob), &actual_size);

	if (status != brain::storage::StorageStatus::kOk ||
		actual_size != sizeof(blob) ||
		blob.magic != kMagic ||
		blob.version != kVersion) {
		gain_trim_a_ = 0;
		gain_trim_b_ = 0;
		offset_trim_a_ = 0;
		offset_trim_b_ = 0;
		return;
	}

	gain_trim_a_ = clamp16(blob.gain_trim_a, kGainTrimMin, kGainTrimMax);
	gain_trim_b_ = clamp16(blob.gain_trim_b, kGainTrimMin, kGainTrimMax);
	offset_trim_a_ = clamp16(blob.offset_trim_a, kOffsetTrimMin, kOffsetTrimMax);
	offset_trim_b_ = clamp16(blob.offset_trim_b, kOffsetTrimMin, kOffsetTrimMax);
}

void Calibration::save_to_app_blob() const {
	CalibrationAppBlobV1 blob{};
	blob.magic = kMagic;
	blob.version = kVersion;
	blob.gain_trim_a = gain_trim_a_;
	blob.gain_trim_b = gain_trim_b_;
	blob.offset_trim_a = offset_trim_a_;
	blob.offset_trim_b = offset_trim_b_;

	const brain::storage::StorageStatus status =
		brain::storage::write_app_blob(&blob, sizeof(blob));
	if (status != brain::storage::StorageStatus::kOk) {
		printf("Calibration app blob save failed: %d\n", static_cast<int>(status));
	}
}
