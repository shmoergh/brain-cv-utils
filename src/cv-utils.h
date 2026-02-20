#ifndef CV_UTILS_H_
#define CV_UTILS_H_

#include <cstdint>

#include "attenuverter.h"
#include "calibration.h"
#include "cv-mixer.h"
#include "precision-adder.h"
#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-ui/button.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

constexpr uint8_t kNumModes = 5;

enum class Mode : uint8_t {
	kAttenuverter = 0,
	kPrecisionAdder = 1,
	kSlew = 2,
	kAdEnvelope = 3,
	kCvMixer = 4
};

class CvUtils {
public:
	CvUtils();

	void init();
	void update();

private:
	// Mode cycling
	void next_mode();
	void set_mode(Mode mode);
	void update_mode_leds();
	void update_mode_led_blink(uint32_t now_us);

	// Calibration mode
	void enter_calibration();
	void exit_calibration();

	// Hardware
	brain::ui::Button button_a_;
	brain::ui::Button button_b_;
	brain::ui::Leds leds_;
	brain::ui::Pots pots_;
	brain::io::AudioCvIn cv_in_;
	brain::io::AudioCvOut cv_out_;
	brain::io::Pulse pulse_;

	// Shared calibration
	Calibration calibration_;

	// Mode handlers
	Attenuverter attenuverter_;
	PrecisionAdder precision_adder_;
	CvMixer cv_mixer_;

	// State
	Mode current_mode_;
	bool button_a_pressed_;
	bool button_b_pressed_;
	bool calibration_active_;
	bool button_a_release_event_;
	uint32_t mode_led_override_started_us_;
	uint32_t mode_led_override_until_us_;

	// Long press detection for entering calibration
	uint32_t both_pressed_since_;  // timestamp when both buttons pressed, 0 if not
	static constexpr uint32_t kLongPressUs = 1500000;  // 1.5 seconds
	static constexpr uint32_t kModeLedBlinkHalfPeriodUs = 100000;  // 100ms
	static constexpr uint32_t kModeLedBlinkCount = 3;
	static constexpr uint32_t kModeLedHoldUs =
		kModeLedBlinkHalfPeriodUs * 2 * kModeLedBlinkCount;
	bool long_press_triggered_;
};

#endif  // CV_UTILS_H_
