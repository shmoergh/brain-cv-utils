#ifndef NOISE_H_
#define NOISE_H_

#include <cstdint>

#include "led-controller.h"
#include "brain-io/audio-cv-out.h"
#include "brain-ui/leds.h"
#include "brain-ui/pots.h"

class Noise {
public:
	Noise();

	void update(brain::ui::Pots& pots, brain::io::AudioCvOut& cv_out,
				bool button_b_pressed, brain::ui::Leds& leds,
				LedController& led_controller);

private:
	// Linear congruential generator
	static uint32_t next_random(uint32_t seed);

	// Convert pot value (0-255) to interval in microseconds (logarithmic)
	// Range: ~1ms (fast) to ~2s (slow)
	static uint32_t pot_to_interval_us(uint8_t pot_value);

	static constexpr uint8_t kPotSpeedA = 0;
	static constexpr uint8_t kPotSpeedB = 1;
	static constexpr uint16_t kDacMax = 4095;

	// Timing range
	static constexpr uint32_t kMinIntervalUs = 1000;      // ~1ms
	static constexpr uint32_t kMaxIntervalUs = 2000000;    // 2s

	// State per channel
	struct ChannelState {
		uint32_t last_update_us;
		uint16_t current_value;
	};

	ChannelState ch_a_;
	ChannelState ch_b_;
	uint32_t rng_state_;
	bool frozen_;
};

#endif  // NOISE_H_
