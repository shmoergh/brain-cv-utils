#ifndef CV_MIXER_H_
#define CV_MIXER_H_

#include <cstdint>

#include "brain/include/inputs.h"
#include "brain/include/leds.h"
#include "brain/include/outputs.h"
#include "brain/include/pots.h"
#include "led-controller.h"

class CvMixer {
public:
	void update(Pots& pots, Inputs& cv_in, Outputs& cv_out, Leds& leds,
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
