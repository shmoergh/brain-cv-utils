#include "noise.h"

#include "pico/time.h"

// Scale note tables: semitone offsets within one octave
const uint8_t Noise::kMajorNotes[] = {0, 2, 4, 5, 7, 9, 11};
const uint8_t Noise::kMinorNotes[] = {0, 2, 3, 5, 7, 8, 10};
const uint8_t Noise::kPentatonicNotes[] = {0, 3, 5, 7, 10};
const uint8_t Noise::kWholeToneNotes[] = {0, 2, 4, 6, 8, 10};

Noise::Noise()
	: rng_state_(123456789),
	  active_scale_(Scale::kUnquantized) {
	ch_a_.last_update_us = 0;
	ch_a_.current_value = kDacCenter;
	ch_b_.last_update_us = 0;
	ch_b_.current_value = kDacCenter;
}

uint32_t Noise::next_random(uint32_t seed) {
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

uint32_t Noise::pot_to_interval_us(uint8_t pot_value) {
	if (pot_value == 0) return kMinIntervalUs;
	uint32_t pot32 = static_cast<uint32_t>(pot_value);
	uint32_t range = kMaxIntervalUs - kMinIntervalUs;
	uint32_t t = (pot32 * pot32 * pot32) / (255UL * 255UL);
	uint32_t interval = kMinIntervalUs + (t * range) / 255UL;
	if (interval > kMaxIntervalUs) interval = kMaxIntervalUs;
	return interval;
}

uint16_t Noise::quantize(uint16_t dac_value) const {
	if (active_scale_ == Scale::kUnquantized) return dac_value;
	if (active_scale_ == Scale::kChromatic) {
		// Snap to nearest semitone
		// dac * 256 / kSemitoneDac256 = semitone index
		uint32_t dac256 = static_cast<uint32_t>(dac_value) * 256;
		uint32_t semitone = (dac256 + kSemitoneDac256 / 2) / kSemitoneDac256;
		uint32_t result = (semitone * kSemitoneDac256) / 256;
		return static_cast<uint16_t>(result > kDacMax ? kDacMax : result);
	}

	// Scale quantization: find nearest note in scale
	const uint8_t* notes;
	uint8_t count;
	switch (active_scale_) {
		case Scale::kMajor:      notes = kMajorNotes;      count = kMajorCount;      break;
		case Scale::kMinor:      notes = kMinorNotes;      count = kMinorCount;      break;
		case Scale::kPentatonic: notes = kPentatonicNotes;  count = kPentatonicCount;  break;
		case Scale::kWholeTone:  notes = kWholeToneNotes;   count = kWholeToneCount;   break;
		default: return dac_value;
	}

	// Find which semitone we're closest to
	uint32_t dac256 = static_cast<uint32_t>(dac_value) * 256;
	uint32_t total_semitones_256 = (dac256 + kSemitoneDac256 / 2) / kSemitoneDac256;
	// total_semitones_256 is the nearest semitone index (0..120)

	uint32_t octave = total_semitones_256 / 12;
	uint32_t semitone_in_octave = total_semitones_256 % 12;

	// Find the closest note in scale
	uint8_t best_note = 0;
	uint32_t best_dist = 12;
	for (uint8_t i = 0; i < count; i++) {
		uint32_t dist;
		if (semitone_in_octave >= notes[i]) {
			dist = semitone_in_octave - notes[i];
		} else {
			dist = notes[i] - semitone_in_octave;
		}
		// Also check wrapping: distance to previous octave's top note
		uint32_t wrap_dist = 12 - dist;
		if (wrap_dist < dist) dist = wrap_dist;
		if (dist < best_dist) {
			best_dist = dist;
			best_note = notes[i];
		}
	}

	uint32_t quantized_semitone = octave * 12 + best_note;
	uint32_t result = (quantized_semitone * kSemitoneDac256) / 256;
	return static_cast<uint16_t>(result > kDacMax ? kDacMax : result);
}

void Noise::render_scale_select(brain::ui::Leds& leds, uint8_t scale_index) {
	leds.off_all();
	if (scale_index < 6) {
		leds.on(scale_index);
	}
}

void Noise::update(brain::ui::Pots& pots, brain::io::AudioCvOut& cv_out,
				   bool button_b_pressed, brain::ui::Leds& leds,
				   LedController& led_controller) {
	uint32_t now = time_us_32();

	// Button B held: pot 3 selects scale, show on LEDs
	if (button_b_pressed) {
		uint8_t pot3 = pots.get(kPotRange);
		uint8_t scale_idx = static_cast<uint8_t>((static_cast<uint16_t>(pot3) * kNumScales) / 256);
		if (scale_idx >= kNumScales) scale_idx = kNumScales - 1;
		active_scale_ = static_cast<Scale>(scale_idx);
		render_scale_select(leds, scale_idx);
		return;  // Don't update random while selecting scale
	}

	// Range from pot 3: 0 = narrow (around center), 255 = full range
	uint8_t range_pot = pots.get(kPotRange);
	// range_half: half the DAC range to use (0..2048)
	uint16_t range_half = static_cast<uint16_t>(
		(static_cast<uint32_t>(range_pot) * kDacCenter) / 255);
	if (range_half < 1) range_half = 1;

	uint32_t interval_a = pot_to_interval_us(pots.get(kPotSpeedA));
	uint32_t interval_b = pot_to_interval_us(pots.get(kPotSpeedB));

	// Channel A
	if ((now - ch_a_.last_update_us) >= interval_a) {
		rng_state_ = next_random(rng_state_);
		// Random in range [center - range_half, center + range_half]
		uint16_t raw = static_cast<uint16_t>(rng_state_ & 0x0FFF);  // 0..4095
		uint16_t scaled = kDacCenter - range_half +
			static_cast<uint16_t>((static_cast<uint32_t>(raw) * range_half * 2) / kDacMax);
		ch_a_.current_value = quantize(scaled);
		ch_a_.last_update_us = now;
	}

	// Channel B
	if ((now - ch_b_.last_update_us) >= interval_b) {
		rng_state_ = next_random(rng_state_);
		uint16_t raw = static_cast<uint16_t>(rng_state_ & 0x0FFF);
		uint16_t scaled = kDacCenter - range_half +
			static_cast<uint16_t>((static_cast<uint32_t>(raw) * range_half * 2) / kDacMax);
		ch_b_.current_value = quantize(scaled);
		ch_b_.last_update_us = now;
	}

	// Output
	float voltage_a = static_cast<float>(ch_a_.current_value) * 10.0f / static_cast<float>(kDacMax);
	float voltage_b = static_cast<float>(ch_b_.current_value) * 10.0f / static_cast<float>(kDacMax);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, voltage_a);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, voltage_b);

	// VU display
	led_controller.render_output_vu(leds, ch_a_.current_value, ch_b_.current_value);
}
