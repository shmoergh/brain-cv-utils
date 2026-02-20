#ifndef CALIBRATION_H_
#define CALIBRATION_H_

#include <cstdint>

#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class Calibration {
public:
	Calibration();

	void init();

	// Getters
	int16_t gain_trim_a() const { return gain_trim_a_; }
	int16_t gain_trim_b() const { return gain_trim_b_; }
	int16_t offset_trim_a() const { return offset_trim_a_; }
	int16_t offset_trim_b() const { return offset_trim_b_; }

	// Update calibration values from pots.
	// base mode: Pot 1 = scale A, Pot 2 = scale B
	// hold Button A + Pot 3 = offset A
	// hold Button B + Pot 3 = offset B
	void update_from_pots(brain::ui::Pots& pots,
						  bool button_a_held, bool button_b_held);

	// Save to flash
	void save();

	// Calibration passthrough: input A->output A, input B->output B.
	// Uses SDK voltage reads and applies live gain/offset trims.
	void process_passthrough(brain::io::AudioCvIn& cv_in,
							 brain::io::AudioCvOut& cv_out) const;

	// Blink all LEDs for calibration mode visual feedback
	void update_leds(brain::ui::Leds& leds);

	// Shared helper for mode LED override timing checks.
	static bool is_mode_led_override_active(uint32_t now_us,
											uint32_t override_until_us);

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
	uint32_t blink_timer_;

	void load_from_flash();
	void save_to_flash();
};

#endif  // CALIBRATION_H_
