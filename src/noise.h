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
	// PRNG (xorshift32)
	static uint32_t next_random(uint32_t seed);

	// Convert pot value (0-255) to interval in microseconds (logarithmic)
	static uint32_t pot_to_interval_us(uint8_t pot_value);

	// Quantize a DAC value (0-4095) to the nearest note in the active scale
	// Returns quantized DAC value
	uint16_t quantize(uint16_t dac_value) const;

	// Show active scale on LEDs (one LED lit per scale)
	static void render_scale_select(brain::ui::Leds& leds, uint8_t scale_index);

	// Scale definitions
	enum class Scale : uint8_t {
		kUnquantized = 0,
		kChromatic,
		kMajor,
		kMinor,
		kPentatonic,
		kWholeTone
	};
	static constexpr uint8_t kNumScales = 6;

	// Note tables: intervals in semitones from root (within one octave)
	// Stored as {note_count, semitone[]}
	static const uint8_t kMajorNotes[];
	static const uint8_t kMinorNotes[];
	static const uint8_t kPentatonicNotes[];
	static const uint8_t kWholeToneNotes[];

	static constexpr uint8_t kMajorCount = 7;
	static constexpr uint8_t kMinorCount = 7;
	static constexpr uint8_t kPentatonicCount = 5;
	static constexpr uint8_t kWholeToneCount = 6;

	// 1V/oct: one semitone in DAC units
	// Full range 0-4095 = 0-10V = 10 octaves, 120 semitones
	// 1 semitone = 4095 / 120 ≈ 34.125 DAC units
	// Use fixed-point: semitone_dac * 256 for precision
	static constexpr uint32_t kSemitoneDac256 = 8736;  // 34.125 * 256

	static constexpr uint8_t kPotSpeedA = 0;
	static constexpr uint8_t kPotSpeedB = 1;
	static constexpr uint8_t kPotRange = 2;
	static constexpr uint16_t kDacMax = 4095;
	static constexpr uint16_t kDacCenter = 2048;

	// Timing range
	static constexpr uint32_t kMinIntervalUs = 1000;
	static constexpr uint32_t kMaxIntervalUs = 2000000;

	// State per channel
	struct ChannelState {
		uint32_t last_update_us;
		uint16_t current_value;
	};

	ChannelState ch_a_;
	ChannelState ch_b_;
	uint32_t rng_state_;
	Scale active_scale_;
};

#endif  // NOISE_H_
