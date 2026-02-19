#ifndef ATTENUVERTER_H_
#define ATTENUVERTER_H_

#include <cstdint>

#include "brain-io/audio-cv-in.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class Attenuverter {
public:
	void update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
				brain::io::AudioCvOut& cv_out, brain::ui::Leds& leds);

private:
	static constexpr uint8_t kPotAttenCh1 = 0;
	static constexpr uint8_t kPotAttenCh2 = 1;
	static constexpr uint8_t kPotDcOffset = 2;
	static constexpr uint8_t kLedCh1 = 4;
	static constexpr uint8_t kLedCh2 = 5;
	static constexpr uint16_t kDacMax = 4095;
	static constexpr uint16_t kDacCenter = 2048;
};

#endif  // ATTENUVERTER_H_
