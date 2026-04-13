#include "cv-utils.h"

#include <stdio.h>

#include "pico/time.h"

CvUtils::CvUtils()
	: current_mode_(Mode::kAttenuverter),
	  button_a_pressed_(false),
	  button_b_pressed_(false),
	  button_a_release_event_(false),
	  initialized_(false) {}

void CvUtils::init() {
	auto init_component = [](BrainInitStatus status, const char* name) {
		if (!brain_init_succeeded(status)) {
			printf("Init failed: %s\n", name);
			return false;
		}
		return true;
	};

	if (!init_component(brain_.init_buttons(), "buttons")) return;
	brain_.buttons.button_a.set_on_press([this]() {
		button_a_pressed_ = true;
	});
	brain_.buttons.button_a.set_on_release([this]() {
		button_a_pressed_ = false;
		button_a_release_event_ = true;
	});
	brain_.buttons.button_b.set_on_press([this]() {
		button_b_pressed_ = true;
	});
	brain_.buttons.button_b.set_on_release([this]() {
		button_b_pressed_ = false;
	});

	if (!init_component(brain_.init_leds(kLedsModePwm), "leds")) return;
	brain_.leds.startup_animation();

	PotsConfig pot_config = create_default_pots_config(3, 8);
	pot_config.simple = false;
	if (!init_component(brain_.init_pots(pot_config), "pots")) return;

	// Inputs DMA keeps ADC round-robin enabled for CV channels, which interferes
	// with the pot mux reader on ADC0. Keep inputs in direct-read mode here.
	if (!init_component(brain_.init_inputs(), "inputs")) return;
	if (!init_component(brain_.init_outputs(), "outputs")) return;

	brain_.outputs.set_output_range(kOutputsChannelA, kOutputsRangeMinus5To5V);
	brain_.outputs.set_output_range(kOutputsChannelB, kOutputsRangeMinus5To5V);

	const bool sdk_calibration_loaded = brain_.outputs.load_calibration_from_flash();
	printf("Storage layout protected: %s\n",
		   brain_.storage.is_layout_protected() ? "yes" : "no");
	printf("CV calibration from SDK storage: %s (has_calibration=%s)\n",
		   sdk_calibration_loaded ? "loaded" : "not found/corrupt (raw fallback)",
		   brain_.outputs.has_calibration() ? "yes" : "no");

	ad_envelope_.init(brain_.inputs);

	set_mode(Mode::kAttenuverter);
	initialized_ = true;
	printf("CV Utils initialized\n");
}

void CvUtils::update() {
	if (!initialized_) {
		return;
	}

	brain_.update_buttons();
	brain_.update_pots();
	brain_.update_inputs();
	brain_.update_leds();

	uint32_t now = time_us_32();

	// --- Button A release: cycle modes ---
	if (button_a_release_event_ && !button_b_pressed_) {
		next_mode();
	}
	button_a_release_event_ = false;

	// --- Dispatch to current mode ---
	switch (current_mode_) {
		case Mode::kAttenuverter:
			attenuverter_.update(brain_.pots, brain_.inputs, brain_.outputs, brain_.leds,
								led_controller_);
			break;
		case Mode::kPrecisionAdder:
			precision_adder_.update(brain_.pots, brain_.inputs, brain_.outputs,
								   button_b_pressed_, brain_.leds, led_controller_);
			break;
		case Mode::kSlew:
			slew_limiter_.update(brain_.pots, brain_.inputs, brain_.outputs,
							 button_b_pressed_, brain_.leds, led_controller_);
			break;
		case Mode::kAdEnvelope:
			ad_envelope_.update(brain_.pots, brain_.inputs, brain_.outputs,
						   button_b_pressed_, brain_.leds, led_controller_);
			break;
		case Mode::kCvMixer:
			cv_mixer_.update(brain_.pots, brain_.inputs, brain_.outputs, brain_.leds,
						 led_controller_);
			break;
		case Mode::kNoise:
			noise_.update(brain_.pots, brain_.inputs, brain_.outputs, button_b_pressed_,
					  brain_.leds, led_controller_);
			break;
	}
	if (led_controller_.is_mode_override_active(now)) {
		led_controller_.render_mode_change(
			brain_.leds, static_cast<uint8_t>(current_mode_), kNumModes, now);
	}
}

// ---------- Mode cycling ----------

void CvUtils::next_mode() {
	uint8_t next = (static_cast<uint8_t>(current_mode_) + 1) % kNumModes;
	set_mode(static_cast<Mode>(next));
	led_controller_.start_mode_change(time_us_32());
	printf("Mode: %d\n", static_cast<int>(current_mode_));
}

void CvUtils::set_mode(Mode mode) {
	current_mode_ = mode;
	brain_.leds.off_all();
}
