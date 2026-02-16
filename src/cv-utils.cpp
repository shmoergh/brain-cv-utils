#include "cv-utils.h"

#include <stdio.h>

#include <algorithm>

#include "brain-common/brain-common.h"

// Attenuverter constants
constexpr uint8_t kPotAttenCh1 = 0;
constexpr uint8_t kPotAttenCh2 = 1;
constexpr uint8_t kPotDcOffset = 2;
constexpr uint8_t kLedOutputCh1 = 4;
constexpr uint8_t kLedOutputCh2 = 5;
constexpr float kMaxOutputVoltage = 10.0f;
constexpr float kMaxDcOffset = 5.0f;

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
	pot_config.simple = true;
	pots_.init(pot_config);

	// Initialize CV I/O
	cv_in_.init();
	cv_out_.init();
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelA, brain::io::AudioCvOutCoupling::kDcCoupled);
	cv_out_.set_coupling(brain::io::AudioCvOutChannel::kChannelB, brain::io::AudioCvOutCoupling::kDcCoupled);

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
			update_attenuverter();
			break;
		case Mode::kPrecisionAdder:
			update_precision_adder();
			break;
		case Mode::kSlew:
			update_slew();
			break;
		case Mode::kAdEnvelope:
			update_ad_envelope();
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
	// LEDs 0-3 indicate current/pending mode (one lit)
	for (uint8_t i = 0; i < kNumModes; i++) {
		if (i == static_cast<uint8_t>(display_mode)) {
			leds_.on(i);
		} else {
			leds_.off(i);
		}
	}
}

// ---------- Mode 1: Attenuverter ----------

void CvUtils::update_attenuverter() {
	// Read pot values (0-255)
	uint16_t pot_ch1 = pots_.get(kPotAttenCh1);
	uint16_t pot_ch2 = pots_.get(kPotAttenCh2);
	uint16_t pot_offset = pots_.get(kPotDcOffset);

	// Convert pots to attenuation factor: -1.0 to +1.0 (center = 0)
	float atten_ch1 = (static_cast<float>(pot_ch1) - 127.5f) / 127.5f;
	float atten_ch2 = (static_cast<float>(pot_ch2) - 127.5f) / 127.5f;

	// DC offset: -5V to +5V (center = 0)
	float dc_offset = (static_cast<float>(pot_offset) - 127.5f) / 127.5f * kMaxDcOffset;

	// Read CV inputs (returns -5V to +5V)
	float cv_in_1 = cv_in_.get_voltage_channel_a();
	float cv_in_2 = cv_in_.get_voltage_channel_b();

	// Apply attenuation and offset
	// Input is -5V to +5V, attenuated then shifted to 0-10V output range
	float out_1 = (cv_in_1 * atten_ch1) + dc_offset + 5.0f;
	float out_2 = (cv_in_2 * atten_ch2) + dc_offset + 5.0f;

	// Clamp to DAC output range (0-10V)
	out_1 = std::clamp(out_1, 0.0f, kMaxOutputVoltage);
	out_2 = std::clamp(out_2, 0.0f, kMaxOutputVoltage);

	// Write to DAC
	cv_out_.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_1);
	cv_out_.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_2);

	// LED feedback: brightness proportional to output voltage
	uint8_t led_brightness_1 = static_cast<uint8_t>(out_1 / kMaxOutputVoltage * 255.0f);
	uint8_t led_brightness_2 = static_cast<uint8_t>(out_2 / kMaxOutputVoltage * 255.0f);
	leds_.set_brightness(kLedOutputCh1, led_brightness_1);
	leds_.set_brightness(kLedOutputCh2, led_brightness_2);
}

// ---------- Mode stubs (Phase 3-5) ----------

void CvUtils::update_precision_adder() {
	// TODO: Phase 3
}

void CvUtils::update_slew() {
	// TODO: Phase 4
}

void CvUtils::update_ad_envelope() {
	// TODO: Phase 5
}

void CvUtils::on_button_a_press() {
	// Dispatch to mode-specific handler
	switch (current_mode_) {
		case Mode::kAttenuverter:
			// No action in attenuverter mode
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
