#ifndef CV_MIXER_H_
#define CV_MIXER_H_

#include <cstdint>

#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class CvMixer {
public:
	void update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
				brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds,
				bool allow_led_updates = true);

private:
	static constexpr uint8_t kPotLevelA = 0;
	static constexpr uint8_t kPotLevelB = 1;
	static constexpr uint8_t kPotMain = 2;
	static constexpr uint8_t kLedsCh1[3] = {0, 1, 2};
	static constexpr uint8_t kLedsCh2[3] = {3, 4, 5};
	static constexpr float kMaxVoltage = 10.0f;
	static constexpr float kCenterVoltage = 5.0f;
	static constexpr float kMinSignalVoltage = -5.0f;
	static constexpr float kMaxSignalVoltage = 5.0f;

	static void update_vu_leds(float voltage, const uint8_t led_indices[3],
							   brain::ui::Leds& leds);
};

#endif  // CV_MIXER_H_
