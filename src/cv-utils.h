#ifndef CV_UTILS_H_
#define CV_UTILS_H_

#include <cstdint>

#include "ad-envelope.h"
#include "attenuverter.h"
#include "cv-mixer.h"
#include "led-controller.h"
#include "noise.h"
#include "precision-adder.h"
#include "slew-limiter.h"
#include "brain/brain.h"

constexpr uint8_t kNumModes = 6;

enum class Mode : uint8_t {
	kAttenuverter = 0,
	kPrecisionAdder = 1,
	kSlew = 2,
	kAdEnvelope = 3,
	kCvMixer = 4,
	kNoise = 5
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

	// Hardware
	Brain brain_;

	LedController led_controller_;

	// Mode handlers
	Attenuverter attenuverter_;
	PrecisionAdder precision_adder_;
	SlewLimiter slew_limiter_;
	AdEnvelope ad_envelope_;
	CvMixer cv_mixer_;
	Noise noise_;

	// State
	Mode current_mode_;
	bool button_a_pressed_;
	bool button_b_pressed_;
	bool button_a_release_event_;
	bool initialized_;
};

#endif  // CV_UTILS_H_
