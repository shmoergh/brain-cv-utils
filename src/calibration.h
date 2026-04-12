#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#include <cstdint>

#include "brain/include/inputs.h"
#include "brain/include/leds.h"
#include "brain/include/outputs.h"
#include "brain/include/pots.h"
#include "brain/include/storage.h"

class Calibration {
public:
	Calibration();

	void init(Storage& storage);

	// Getters
	int16_t gain_trim_a() const { return gain_trim_a_; }
	int16_t gain_trim_b() const { return gain_trim_b_; }
	int16_t offset_trim_a() const { return offset_trim_a_; }
	int16_t offset_trim_b() const { return offset_trim_b_; }

	// Update calibration values from pots.
	// base mode: Pot 1 = scale A, Pot 2 = scale B
	// hold Button A + Pot 3 = offset A
	// hold Button B + Pot 3 = offset B
	void update_from_pots(Pots& pots, bool button_a_held, bool button_b_held);

	// Save app-level trim state to SDK app blob storage
	void save();

	// Calibration passthrough: input A->output A, input B->output B.
	// Uses SDK voltage reads and applies live gain/offset trims.
	void process_passthrough(Inputs& inputs, Outputs& outputs) const;

	// Blink all LEDs for calibration mode visual feedback
	void update_leds(Leds& leds);

	// Constants for modes that apply calibration
	static constexpr int32_t kCalibScale = 10000;

private:
	static constexpr int16_t kGainTrimMin = -300;    // -3.00%
	static constexpr int16_t kGainTrimMax = 300;     // +3.00%
	static constexpr int16_t kOffsetTrimMin = -200;  // ~-0.5V in DAC units
	static constexpr int16_t kOffsetTrimMax = 200;   // ~+0.5V in DAC units

	int16_t gain_trim_a_;
	int16_t gain_trim_b_;
	int16_t offset_trim_a_;
	int16_t offset_trim_b_;
	Storage* storage_ = nullptr;

	void load_from_app_blob();
	void save_to_app_blob() const;
};

#endif  // CALIBRATION_H_
