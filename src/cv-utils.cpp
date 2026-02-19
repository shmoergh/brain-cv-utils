#include "cv-utils.h"

#include <stdio.h>

#include "brain-common/brain-common.h"

CvUtils::CvUtils()
	: button_a_(BRAIN_BUTTON_1),
	  button_b_(BRAIN_BUTTON_2),
	  current_mode_(Mode::kAttenuverter),
	  mode_select_active_(false),
	  pending_mode_(Mode::kAttenuverter) {}

void CvUtils::init() {
	// Initialize buttons
	button_a_.init();
	button_b_.init();

	// Button A: mode-specific action, or cycle mode when in mode select
	button_a_.set_on_press([this]() {
		if (mode_select_active_) {
			next_mode();
		} else {
			on_button_a_press();
		}
	});

	// Button B: hold to enter mode select, release to confirm
	button_b_.set_on_press([this]() {
		enter_mode_select();
	});

	button_b_.set_on_release([this]() {
		if (mode_select_active_) {
			exit_mode_select();
		}
	});

	// Initialize LEDs
	leds_.init();
	leds_.startup_animation();

	// Initialize pots (3 pots, 8-bit resolution for smooth control)
	brain::ui::PotsConfig pot_config = brain::ui::create_default_config(3, 8);
	pot_config.simple = false;
	pots_.init(pot_config);

	// Initialize CV I/O
	cv_in_.init();
	cv_out_.init();
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelA, brain::io::AudioCvOutCoupling::kAcCoupled);
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelB, brain::io::AudioCvOutCoupling::kAcCoupled);

	// Initialize pulse I/O
	pulse_.begin();

	// Set initial mode
	set_mode(Mode::kAttenuverter);

	printf("CV Utils initialized\n");
}

void CvUtils::update() {
	// Poll hardware
	button_a_.update();
	button_b_.update();
	pots_.scan();
	cv_in_.update();

	// When in mode select, only handle mode switching (no CV processing)
	if (mode_select_active_) {
		return;
	}

	// Dispatch to current mode
	switch (current_mode_) {
		case Mode::kAttenuverter:
			attenuverter_.update(pots_, cv_in_, cv_out_, leds_);
			break;
		case Mode::kPrecisionAdder:
			// TODO: Phase 3
			break;
		case Mode::kSlew:
			// TODO: Phase 4
			break;
		case Mode::kAdEnvelope:
			// TODO: Phase 5
			break;
	}
}

// ---------- Mode select ----------

void CvUtils::enter_mode_select() {
	mode_select_active_ = true;
	pending_mode_ = current_mode_;
	update_mode_leds();
	printf("Mode select: hold B, tap A to switch\n");
}

void CvUtils::exit_mode_select() {
	mode_select_active_ = false;
	set_mode(pending_mode_);
	printf("Mode confirmed: %d\n", static_cast<int>(current_mode_));
}

void CvUtils::next_mode() {
	uint8_t next = (static_cast<uint8_t>(pending_mode_) + 1) % kNumModes;
	pending_mode_ = static_cast<Mode>(next);
	update_mode_leds();
	printf("Mode preview: %d\n", static_cast<int>(pending_mode_));
}

void CvUtils::set_mode(Mode mode) {
	current_mode_ = mode;
	pending_mode_ = mode;
	update_mode_leds();
}

void CvUtils::update_mode_leds() {
	Mode display_mode = mode_select_active_ ? pending_mode_ : current_mode_;
	for (uint8_t i = 0; i < kNumModes; i++) {
		if (i == static_cast<uint8_t>(display_mode)) {
			leds_.on(i);
		} else {
			leds_.off(i);
		}
	}
}

// ---------- Button A ----------

void CvUtils::on_button_a_press() {
	switch (current_mode_) {
		case Mode::kAttenuverter:
			break;
		case Mode::kPrecisionAdder:
			// TODO: Reset offsets to 0V
			break;
		case Mode::kSlew:
			// TODO: Toggle linked mode
			break;
		case Mode::kAdEnvelope:
			// TODO: Manual trigger
			break;
	}
}
