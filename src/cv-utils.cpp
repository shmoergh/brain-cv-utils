#include "cv-utils.h"

#include <stdio.h>

#include "brain-common/brain-common.h"

CvUtils::CvUtils()
	: button_a_(BRAIN_BUTTON_1),
	  button_b_(BRAIN_BUTTON_2),
	  current_mode_(Mode::kAttenuverter),
	  mode_select_active_(false),
	  pending_mode_(Mode::kAttenuverter),
	  button_a_pressed_(false),
	  button_b_pressed_(false) {}

void CvUtils::init() {
	// Initialize buttons
	button_a_.init();
	button_b_.init();

	// Button A: press/release track state, enter mode select if B already held
	button_a_.set_on_press([this]() {
		button_a_pressed_ = true;
		if (button_b_pressed_ && !mode_select_active_) {
			enter_mode_select();
		}
	});
	button_a_.set_on_release([this]() {
		button_a_pressed_ = false;
		exit_mode_select();
	});

	// Button B: press/release track state, enter mode select if A already held
	button_b_.set_on_press([this]() {
		button_b_pressed_ = true;
		if (button_a_pressed_ && !mode_select_active_) {
			enter_mode_select();
		}
	});
	button_b_.set_on_release([this]() {
		button_b_pressed_ = false;
		exit_mode_select();
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
	precision_adder_.init();

	printf("CV Utils initialized\n");
}

void CvUtils::update() {
	// Poll hardware
	button_a_.update();
	button_b_.update();
	pots_.scan();
	cv_in_.update();

	// When in mode select, read POT1 to choose mode
	if (mode_select_active_) {
		Mode new_mode = pot_to_mode(pots_.get(0));
		if (new_mode != pending_mode_) {
			pending_mode_ = new_mode;
			update_mode_leds();
		}
		return;
	}

	// Dispatch to current mode
	switch (current_mode_) {
		case Mode::kAttenuverter:
			attenuverter_.update(pots_, cv_in_, cv_out_, leds_);
			break;
		case Mode::kPrecisionAdder:
			precision_adder_.update(pots_, cv_in_, cv_out_, leds_, button_a_pressed_, button_b_pressed_);
			break;
		case Mode::kSlew:
			// TODO: Phase 4
			break;
		case Mode::kAdEnvelope:
			// TODO: Phase 5
			break;
		case Mode::kCvMixer:
			cv_mixer_.update(pots_, cv_in_, cv_out_, leds_);
			break;
	}
}

// ---------- Mode select ----------

void CvUtils::enter_mode_select() {
	mode_select_active_ = true;
	pending_mode_ = current_mode_;
	leds_.off_all();
	update_mode_leds();
	printf("Mode select: turn POT1 to choose mode\n");
}

void CvUtils::exit_mode_select() {
	// Only exit when both buttons are released
	if (button_a_pressed_ || button_b_pressed_) {
		return;
	}
	if (!mode_select_active_) {
		return;
	}
	mode_select_active_ = false;
	leds_.off_all();
	set_mode(pending_mode_);
	printf("Mode confirmed: %d\n", static_cast<int>(current_mode_));
}

Mode CvUtils::pot_to_mode(uint8_t pot_value) {
	// 5 modes: divide 0-255 into 5 zones of ~51 each
	if (pot_value < 51) return Mode::kAttenuverter;
	if (pot_value < 102) return Mode::kPrecisionAdder;
	if (pot_value < 153) return Mode::kSlew;
	if (pot_value < 204) return Mode::kAdEnvelope;
	return Mode::kCvMixer;
}

void CvUtils::set_mode(Mode mode) {
	current_mode_ = mode;
	pending_mode_ = mode;
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
