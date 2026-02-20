#include "slew-limiter.h"
#include "fixed-point.h"

#include <pico/time.h>
#include <cstdio>

namespace {
// Internal fixed-point format:
// - Voltages are represented as signed millivolts (mV).
// - Fractions are represented as Q15 (0..32768 == 0.0..1.0).
constexpr int32_t kMillivoltsPerVolt = 1000;
constexpr uint32_t kPotMax = 255;
constexpr uint32_t kPotCubeMax = kPotMax * kPotMax * kPotMax;
constexpr uint32_t kMinSlewDenominatorUs = 2000;  // Mirrors old 0.001f threshold.
constexpr bool kEnableSlewDebug = true;
constexpr uint32_t kSlewDebugPeriodUs = 100000;  // 10 Hz

uint16_t pot_to_slew_rate_q15(uint8_t pot_value) {
	if (pot_value == 0) return 0;
	const uint32_t p = pot_value;
	const uint32_t p3 = p * p * p;
	// Cubic taper keeps more knob travel in slower slew region.
	const uint64_t scaled =
		(static_cast<uint64_t>(p3) * fixed_point::kQ15One + (kPotCubeMax / 2)) / kPotCubeMax;
	return static_cast<uint16_t>(scaled > fixed_point::kQ15One ? fixed_point::kQ15One : scaled);
}

uint16_t pot_to_shape_q15(uint8_t pot_value) {
	return fixed_point::u8_to_q15(pot_value);
}
}

SlewLimiter::SlewLimiter()
	: current_ch1_mv_(0),
	  current_ch2_mv_(0),
	  output_smoother_ch1_(kOutputDeadbandMv, kOutputSmoothingAlphaQ15),
	  output_smoother_ch2_(kOutputDeadbandMv, kOutputSmoothingAlphaQ15),
	  last_time_us_(0),
	  linked_(false),
	  button_b_prev_(false) {}

void SlewLimiter::update(brain::ui::Pots& pots, brain::io::AudioCvIn& cv_in,
						  brain::io::AudioCvOut& cv_out,
						  Calibration& calibration, bool button_b_pressed,
						  brain::ui::Leds& leds, LedController& led_controller) {
	(void)calibration;

	// Button B release: toggle linked mode
	if (button_b_prev_ && !button_b_pressed) {
		linked_ = !linked_;
	}
	button_b_prev_ = button_b_pressed;

	// Delta time
	uint32_t now_us = time_us_32();
	uint32_t dt_us = now_us - last_time_us_;
	last_time_us_ = now_us;
	if (dt_us > 100000) dt_us = 100000;

	// Pots
	const uint16_t rise_rate_q15 = pot_to_slew_rate_q15(pots.get(kPotRise));
	const uint16_t fall_rate_q15 = linked_ ? rise_rate_q15 : pot_to_slew_rate_q15(pots.get(kPotFall));
	const uint16_t shape_q15 = pot_to_shape_q15(pots.get(kPotShape));
	const auto compute_coeff_q15 = [dt_us](uint16_t rate_q15, uint32_t max_slew_us) -> uint16_t {
		if (rate_q15 == 0) return fixed_point::kQ15One;
		// coeff ~= dt / (rate * max_slew_time), clamped to [0, 1] in Q15.
		const uint64_t denominator =
			(static_cast<uint64_t>(rate_q15) * max_slew_us) / fixed_point::kQ15One;
		if (denominator <= kMinSlewDenominatorUs) return fixed_point::kQ15One;
		const uint64_t numerator = static_cast<uint64_t>(dt_us) * fixed_point::kQ15One;
		const uint64_t scaled = (numerator + (denominator / 2)) / denominator;
		return static_cast<uint16_t>(scaled >= fixed_point::kQ15One ? fixed_point::kQ15One : scaled);
	};

	// Slew coefficients
	const uint16_t rise_coeff_q15 = compute_coeff_q15(rise_rate_q15, kMaxSlewUs);
	const uint16_t fall_coeff_q15 = compute_coeff_q15(fall_rate_q15, kMaxSlewUs);

	// Read inputs and apply slew
	const float in_ch1_v = cv_in.get_voltage_channel_a();
	const float in_ch2_v = cv_in.get_voltage_channel_b();
	const int32_t in_ch1_mv_unclamped =
		static_cast<int32_t>(in_ch1_v * static_cast<float>(kMillivoltsPerVolt));
	const int32_t in_ch2_mv_unclamped =
		static_cast<int32_t>(in_ch2_v * static_cast<float>(kMillivoltsPerVolt));
	const int32_t in_ch1_mv = fixed_point::clamp_i32(
		in_ch1_mv_unclamped,
		kMinSignalMillivolts, kMaxSignalMillivolts);
	const int32_t in_ch2_mv = fixed_point::clamp_i32(
		in_ch2_mv_unclamped,
		kMinSignalMillivolts, kMaxSignalMillivolts);
	current_ch1_mv_ =
		slew_channel_mv(in_ch1_mv, current_ch1_mv_, rise_coeff_q15, fall_coeff_q15, shape_q15);
	current_ch2_mv_ =
		slew_channel_mv(in_ch2_mv, current_ch2_mv_, rise_coeff_q15, fall_coeff_q15, shape_q15);

	// Map bipolar signal (-5V..+5V) into DAC domain (0V..10V) around 5V center.
	const int32_t target_a_mv =
		fixed_point::clamp_i32(current_ch1_mv_ + kCenterMillivolts, 0, kMaxMillivolts);
	const int32_t target_b_mv =
		fixed_point::clamp_i32(current_ch2_mv_ + kCenterMillivolts, 0, kMaxMillivolts);
	const int32_t out_a_mv = output_smoother_ch1_.process(target_a_mv);
	const int32_t out_b_mv = output_smoother_ch2_.process(target_b_mv);
	const float target_a_voltage = static_cast<float>(target_a_mv) / static_cast<float>(kMillivoltsPerVolt);
	const float target_b_voltage = static_cast<float>(target_b_mv) / static_cast<float>(kMillivoltsPerVolt);
	const float out_a_voltage = static_cast<float>(out_a_mv) / static_cast<float>(kMillivoltsPerVolt);
	const float out_b_voltage = static_cast<float>(out_b_mv) / static_cast<float>(kMillivoltsPerVolt);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, out_a_voltage);
	cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, out_b_voltage);
	led_controller.render_output_vu(leds, out_a_voltage, out_b_voltage);

	if (kEnableSlewDebug) {
		static uint32_t last_debug_us = 0;
		if ((now_us - last_debug_us) >= kSlewDebugPeriodUs) {
			last_debug_us = now_us;
			printf(
				"\r\033[2K[slew A] raw=%4u in_v=%+7.3f in_mv=%+6ld cur_mv=%+6ld target_v=%+7.3f smooth_v=%+7.3f\n"
				"\r\033[2K[slew B] raw=%4u in_v=%+7.3f in_mv=%+6ld cur_mv=%+6ld target_v=%+7.3f smooth_v=%+7.3f\033[1A\r",
				cv_in.get_raw_channel_a(), in_ch1_v, static_cast<long>(in_ch1_mv),
				static_cast<long>(current_ch1_mv_), target_a_voltage, out_a_voltage,
				cv_in.get_raw_channel_b(), in_ch2_v, static_cast<long>(in_ch2_mv),
				static_cast<long>(current_ch2_mv_), target_b_voltage, out_b_voltage);
			fflush(stdout);
		}
	}
}

int32_t SlewLimiter::slew_channel_mv(int32_t input_mv, int32_t current_mv,
									 uint16_t rise_coeff_q15,
									 uint16_t fall_coeff_q15,
									 uint16_t shape_q15) {
	const int32_t diff_mv = input_mv - current_mv;
	if (diff_mv == 0) return current_mv;

	const uint16_t coeff_q15 = diff_mv > 0 ? rise_coeff_q15 : fall_coeff_q15;

	// Exponential: fraction of remaining distance.
	const int32_t exp_step_mv = fixed_point::mul_q15(diff_mv, coeff_q15);

	// Linear: constant rate in millivolts.
	// kMaxMillivolts scales the "constant-rate" branch to full-range per coeff=1.
	int32_t linear_move_mv = fixed_point::mul_q15(kMaxMillivolts, coeff_q15);
	if (diff_mv < 0) linear_move_mv = -linear_move_mv;
	if ((diff_mv > 0 && linear_move_mv > diff_mv) || (diff_mv < 0 && linear_move_mv < diff_mv)) {
		linear_move_mv = diff_mv;
	}

	// Blend: shape=0 -> linear, shape=1 -> exponential.
	int32_t step_mv = fixed_point::blend_q15(linear_move_mv, exp_step_mv, shape_q15);
	// Avoid coefficient-dependent deadband from integer truncation at very small diffs.
	if (step_mv == 0) {
		step_mv = diff_mv > 0 ? 1 : -1;
	}
	return current_mv + step_mv;
}
