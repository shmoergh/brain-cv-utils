#ifndef CV_MIXER_H_
#define CV_MIXER_H_

#include <cstdint>

#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"
#include "led-controller.h"

class CvMixer {
public:
	void update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
				brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
				LedController& led_controller);

private:
	static constexpr uint8_t kPotLevelA = 0;
	static constexpr uint8_t kPotLevelB = 1;
	static constexpr uint8_t kPotMain = 2;
	static constexpr float kMaxVoltage = 10.0f;
	static constexpr float kCenterVoltage = 5.0f;
	static constexpr float kMinSignalVoltage = -5.0f;
	static constexpr float kMaxSignalVoltage = 5.0f;
};

#endif  // CV_MIXER_H_
