#include "slew-limiter.h"
#include "fixed-point.h"

#include <pico/time.h>
#include <cstdio>

namespace {
// Internal fixed-point format:
// - Voltages are represented as signed millivolts (mV).
// - Fractions are represented as Q15 (0..32768 == 0.0..1.0).
constexpr int32_t kMillivoltsPerVolt = 1000;
constexpr int32_t kDacMax = 4095;
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

int32_t pot_to_offset_mv(uint8_t pot_value, int32_t max_mv) {
	return (static_cast<int32_t>(pot_value) * max_mv + 127) / 255;
}
}

SlewLimiter::SlewLimiter()
	: current_ch1_mv_(0),
	  current_ch2_mv_(0),
	  output_smoother_ch1_(kOutputDeadbandMv, kOutputSmoothingAlphaQ15),
	  output_smoother_ch2_(kOutputDeadbandMv, kOutputSmoothingAlphaQ15),
	  last_time_us_(0),
	  slew_dt_accum_us_(0),
	  linked_(false),
	  button_b_prev_(false) {}

void SlewLimiter::update(Pots& pots, Inputs& cv_in, Outputs& cv_out,
						  bool button_b_pressed,
						  Leds& leds, LedController& led_controller) {
	// Button B release: toggle linked mode
	if (button_b_prev_ && !button_b_pressed) {
		linked_ = !linked_;
	}
	button_b_prev_ = button_b_pressed;

	// Accumulate elapsed time. The slew step itself runs at a fixed period
	// (kSlewStepPeriodUs) regardless of main-loop rate — at the SDK 2.1 main-loop
	// rate (tens of kHz) per-tick coefficients would otherwise round to 0 in Q15.
	const uint32_t now_us = time_us_32();
	uint32_t loop_dt_us = now_us - last_time_us_;
	last_time_us_ = now_us;
	if (loop_dt_us > 100000) loop_dt_us = 100000;
	slew_dt_accum_us_ += loop_dt_us;

	// Read inputs, add the Pot 3 positive offset, and clamp into the unipolar
	// 0..10V output range. Negative inputs (the SDK always returns bipolar mV)
	// are folded against the offset and clamped to 0V if still below.
	const int32_t in_ch1_mv_raw = cv_in.get_voltage_millivolts_channel_a();
	const int32_t in_ch2_mv_raw = cv_in.get_voltage_millivolts_channel_b();
	const int32_t offset_mv = pot_to_offset_mv(pots.get(kPotOffset), kOffsetMaxMillivolts);
	const int32_t in_ch1_mv =
		fixed_point::clamp_i32(in_ch1_mv_raw + offset_mv, 0, kMaxMillivolts);
	const int32_t in_ch2_mv =
		fixed_point::clamp_i32(in_ch2_mv_raw + offset_mv, 0, kMaxMillivolts);

	if (slew_dt_accum_us_ >= kSlewStepPeriodUs) {
		uint32_t step_dt_us = slew_dt_accum_us_;
		if (step_dt_us > 100000) step_dt_us = 100000;
		slew_dt_accum_us_ = 0;

		const uint16_t rise_rate_q15 = pot_to_slew_rate_q15(pots.get(kPotRise));
		const uint16_t fall_rate_q15 =
			linked_ ? rise_rate_q15 : pot_to_slew_rate_q15(pots.get(kPotFall));

		const auto compute_coeff_q15 = [step_dt_us](
			uint16_t rate_q15, uint32_t max_slew_us) -> uint16_t {
			if (rate_q15 == 0) return fixed_point::kQ15One;
			// coeff ~= dt / (rate * max_slew_time), clamped to [0, 1] in Q15.
			const uint64_t denominator =
				(static_cast<uint64_t>(rate_q15) * max_slew_us) / fixed_point::kQ15One;
			if (denominator <= kMinSlewDenominatorUs) return fixed_point::kQ15One;
			const uint64_t numerator = static_cast<uint64_t>(step_dt_us) * fixed_point::kQ15One;
			const uint64_t scaled = (numerator + (denominator / 2)) / denominator;
			return static_cast<uint16_t>(
				scaled >= fixed_point::kQ15One ? fixed_point::kQ15One : scaled);
		};

		const uint16_t rise_coeff_q15 = compute_coeff_q15(rise_rate_q15, kMaxSlewUs);
		const uint16_t fall_coeff_q15 = compute_coeff_q15(fall_rate_q15, kMaxSlewUs);

		current_ch1_mv_ =
			slew_channel_mv(in_ch1_mv, current_ch1_mv_, rise_coeff_q15, fall_coeff_q15);
		current_ch2_mv_ =
			slew_channel_mv(in_ch2_mv, current_ch2_mv_, rise_coeff_q15, fall_coeff_q15);
	}

	// current_ch?_mv_ is already in the unipolar 0..10V DAC domain.
	const int32_t target_a_mv = fixed_point::clamp_i32(current_ch1_mv_, 0, kMaxMillivolts);
	const int32_t target_b_mv = fixed_point::clamp_i32(current_ch2_mv_, 0, kMaxMillivolts);

	int32_t dac_a = (target_a_mv * kDacMax + (kMaxMillivolts / 2)) / kMaxMillivolts;
	int32_t dac_b = (target_b_mv * kDacMax + (kMaxMillivolts / 2)) / kMaxMillivolts;
	dac_a = fixed_point::clamp_i32(dac_a, 0, kDacMax);
	dac_b = fixed_point::clamp_i32(dac_b, 0, kDacMax);

	const int32_t calibrated_target_a_mv = (dac_a * kMaxMillivolts + (kDacMax / 2)) / kDacMax;
	const int32_t calibrated_target_b_mv = (dac_b * kMaxMillivolts + (kDacMax / 2)) / kDacMax;
	const int32_t out_a_mv = output_smoother_ch1_.process(calibrated_target_a_mv);
	const int32_t out_b_mv = output_smoother_ch2_.process(calibrated_target_b_mv);
	const float target_a_voltage =
		static_cast<float>(calibrated_target_a_mv) / static_cast<float>(kMillivoltsPerVolt);
	const float target_b_voltage =
		static_cast<float>(calibrated_target_b_mv) / static_cast<float>(kMillivoltsPerVolt);
	const float out_a_voltage = static_cast<float>(out_a_mv) / static_cast<float>(kMillivoltsPerVolt);
	const float out_b_voltage = static_cast<float>(out_b_mv) / static_cast<float>(kMillivoltsPerVolt);
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelA, out_a_mv);
	cv_out.set_voltage_calibrated_millivolts(kOutputsChannelB, out_b_mv);
	led_controller.render_output_vu_unipolar(leds, out_a_voltage, out_b_voltage);

	if (kEnableSlewDebug) {
		static uint32_t last_debug_us = 0;
		if ((now_us - last_debug_us) >= kSlewDebugPeriodUs) {
			last_debug_us = now_us;
			printf(
				"\r\033[2K[slew A] raw=%4u in_mv=%+6ld cur_mv=%+6ld target_v=%+7.3f smooth_v=%+7.3f\n"
				"\r\033[2K[slew B] raw=%4u in_mv=%+6ld cur_mv=%+6ld target_v=%+7.3f smooth_v=%+7.3f\033[1A\r",
				cv_in.get_raw_channel_a(), static_cast<long>(in_ch1_mv),
				static_cast<long>(current_ch1_mv_), target_a_voltage, out_a_voltage,
				cv_in.get_raw_channel_b(), static_cast<long>(in_ch2_mv),
				static_cast<long>(current_ch2_mv_), target_b_voltage, out_b_voltage);
			fflush(stdout);
		}
	}
}

int32_t SlewLimiter::slew_channel_mv(int32_t input_mv, int32_t current_mv,
									 uint16_t rise_coeff_q15,
									 uint16_t fall_coeff_q15) {
	const int32_t diff_mv = input_mv - current_mv;
	if (diff_mv == 0) return current_mv;

	const uint16_t coeff_q15 = diff_mv > 0 ? rise_coeff_q15 : fall_coeff_q15;

	// Exponential: per-tick step is a fixed fraction of the remaining distance.
	int32_t step_mv = fixed_point::mul_q15(diff_mv, coeff_q15);
	// Floor: ensure we keep moving toward the target even when integer
	// truncation would otherwise zero out the step at very small diffs.
	if (step_mv == 0) {
		step_mv = diff_mv > 0 ? 1 : -1;
	}
	return current_mv + step_mv;
}
