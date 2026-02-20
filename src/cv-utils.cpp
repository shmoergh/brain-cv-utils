#include "cv-utils.h"

#include <stdio.h>

#include "brain-common/brain-common.h"
#include "pico/time.h"

CvUtils::CvUtils()
	: button_a_(BRAIN_BUTTON_1),
	  button_b_(BRAIN_BUTTON_2),
	  current_mode_(Mode::kAttenuverter),
	  button_a_pressed_(false),
	  button_b_pressed_(false),
	  calibration_active_(false),
	  button_a_release_event_(false),
	  mode_led_override_started_us_(0),
	  mode_led_override_until_us_(0),
	  both_pressed_since_(0),
	  long_press_triggered_(false) {}

void CvUtils::init() {
	// Initialize buttons
	button_a_.init();
	button_b_.init();

	// Track button press/release state
	button_a_.set_on_press([this]() {
		button_a_pressed_ = true;
	});
	button_a_.set_on_release([this]() {
		button_a_pressed_ = false;
		button_a_release_event_ = true;
	});
	button_b_.set_on_press([this]() {
		button_b_pressed_ = true;
	});
	button_b_.set_on_release([this]() {
		button_b_pressed_ = false;
	});

	// Initialize LEDs
	leds_.init();
	leds_.startup_animation();

	// Initialize pots (3 pots, 8-bit resolution, smoothing enabled)
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

	// Load calibration from flash
	calibration_.init();

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

	uint32_t now = time_us_32();

	// --- Long press detection for calibration mode ---
	if (button_a_pressed_ && button_b_pressed_) {
		if (both_pressed_since_ == 0) {
			both_pressed_since_ = now;
			long_press_triggered_ = false;
		}

		if (!calibration_active_ && !long_press_triggered_) {
			uint32_t held_us = now - both_pressed_since_;
			if (held_us >= kLongPressUs) {
				long_press_triggered_ = true;
				enter_calibration();
				return;
			}
		}
	} else {
		// At least one button released
		if (both_pressed_since_ != 0 && !long_press_triggered_) {
			// Short tap of both buttons
			if (calibration_active_) {
				exit_calibration();
			}
		}
		both_pressed_since_ = 0;
		long_press_triggered_ = false;
	}

	// --- Calibration mode ---
	if (calibration_active_) {
		calibration_.update_from_pots(pots_, button_a_pressed_, button_b_pressed_);
		calibration_.process_passthrough(cv_in_, cv_out_);
		calibration_.update_leds(leds_);
		button_a_release_event_ = false;
		return;
	}

	// --- Button A release: cycle modes ---
	if (button_a_release_event_ && !button_b_pressed_) {
		next_mode();
	}
	button_a_release_event_ = false;
	const bool mode_led_override_active =
		Calibration::is_mode_led_override_active(now, mode_led_override_until_us_);

	// --- Dispatch to current mode ---
	switch (current_mode_) {
		case Mode::kAttenuverter:
			attenuverter_.update(pots_, cv_in_, cv_out_, leds_,
								!mode_led_override_active);
			break;
		case Mode::kPrecisionAdder:
			precision_adder_.update(pots_, cv_in_, cv_out_, leds_, calibration_,
									button_b_pressed_, !mode_led_override_active);
			break;
		case Mode::kSlew:
			// TODO: Phase 4 (slew limiter)
			break;
		case Mode::kAdEnvelope:
			// TODO: Phase 5
			break;
		case Mode::kCvMixer:
			cv_mixer_.update(pots_, cv_in_, cv_out_, leds_,
							 !mode_led_override_active);
			break;
	}

	// Mode indicator takes precedence briefly after switching modes.
	if (mode_led_override_active) {
		update_mode_led_blink(now);
	}
}

// ---------- Mode cycling ----------

void CvUtils::next_mode() {
	uint8_t next = (static_cast<uint8_t>(current_mode_) + 1) % kNumModes;
	set_mode(static_cast<Mode>(next));
	mode_led_override_started_us_ = time_us_32();
	mode_led_override_until_us_ = mode_led_override_started_us_ + kModeLedHoldUs;
	printf("Mode: %d\n", static_cast<int>(current_mode_));
}

void CvUtils::set_mode(Mode mode) {
	current_mode_ = mode;
	leds_.off_all();
}

void CvUtils::update_mode_leds() {
	// Show current mode LED briefly (modes will take over LEDs on next update)
	for (uint8_t i = 0; i < kNumModes; i++) {
		if (i == static_cast<uint8_t>(current_mode_)) {
			leds_.on(i);
		} else {
			leds_.off(i);
		}
	}
}

void CvUtils::update_mode_led_blink(uint32_t now_us) {
	const uint32_t elapsed = now_us - mode_led_override_started_us_;
	const uint32_t phase = elapsed / kModeLedBlinkHalfPeriodUs;
	const bool led_on = (phase % 2u) == 0u;

	if (!led_on) {
		leds_.off_all();
		return;
	}

	update_mode_leds();
}

// ---------- Calibration mode ----------

void CvUtils::enter_calibration() {
	calibration_active_ = true;
	mode_led_override_started_us_ = 0;
	mode_led_override_until_us_ = 0;
	button_a_release_event_ = false;
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelA, brain::io::AudioCvOutCoupling::kDcCoupled);
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelB, brain::io::AudioCvOutCoupling::kDcCoupled);
	leds_.off_all();
	printf("Calibration mode entered\n");
}

void CvUtils::exit_calibration() {
	calibration_active_ = false;
	mode_led_override_started_us_ = 0;
	button_a_release_event_ = false;
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelA, brain::io::AudioCvOutCoupling::kAcCoupled);
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelB, brain::io::AudioCvOutCoupling::kAcCoupled);
	calibration_.save();
	leds_.off_all();
	printf("Calibration saved, exiting\n");
}
