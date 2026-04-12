#ifndef ATTENUVERTER_H_
#define ATTENUVERTER_H_

#include <cstdint>

#include "brain/include/inputs.h"
#include "brain/include/leds.h"
#include "brain/include/outputs.h"
#include "brain/include/pots.h"
#include "led-controller.h"

class Attenuverter {
public:
	void update(Pots& pots, Inputs& cv_in, Outputs& cv_out, Leds& leds,
				LedController& led_controller);

private:
	static constexpr uint8_t kPotAttenCh1 = 0;
	static constexpr uint8_t kPotAttenCh2 = 1;
	static constexpr uint8_t kPotDcOffset = 2;
	static constexpr uint16_t kDacMax = 4095;
	static constexpr uint16_t kDacCenter = 2048;
};

#endif  // ATTENUVERTER_H_
