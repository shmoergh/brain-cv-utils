#ifndef CV_UTILS_H_
#define CV_UTILS_H_

#include <cstdint>

#include "attenuverter.h"
#include "precision-adder.h"
#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-ui/button.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

constexpr uint8_t kNumModes = 4;

enum class Mode : uint8_t {
	kAttenuverter = 0,
	kPrecisionAdder = 1,
	kSlew = 2,
	kAdEnvelope = 3
};

class CvUtils {
public:
	CvUtils();

	void init();
	void update();

private:
	// Mode switching
	void enter_mode_select();
	void exit_mode_select();
	void set_mode(Mode mode);
	void update_mode_leds();
	static Mode pot_to_mode(uint8_t pot_value);

	// Hardware
	brain::ui::Button button_a_;
	brain::ui::Button button_b_;
	brain::ui::Leds leds_;
	brain::ui::Pots pots_;
	brain::io::AudioCvIn cv_in_;
	brain::io::AudioCvOut cv_out_;
	brain::io::Pulse pulse_;

	// Mode handlers
	Attenuverter attenuverter_;
	PrecisionAdder precision_adder_;

	// State
	Mode current_mode_;
	bool mode_select_active_;
	Mode pending_mode_;
	bool button_a_pressed_;
	bool button_b_pressed_;
};

#endif  // CV_UTILS_H_
