#include <cassert>
#include <cstdint>
#include <cstdio>

#include "../src/fixed-point.h"

namespace {
constexpr int32_t kMaxMillivolts = 10000;
constexpr int32_t kCenterMillivolts = 5000;
constexpr uint32_t kPotMax = 255;
constexpr uint32_t kPotCubeMax = kPotMax * kPotMax * kPotMax;
constexpr uint32_t kMinSlewDenominatorUs = 2000;
constexpr uint32_t kMaxSlewUs = 2000000;

uint16_t pot_to_slew_rate_q15(uint8_t pot_value) {
	if (pot_value == 0) return 0;
	const uint32_t p = pot_value;
	const uint32_t p3 = p * p * p;
	const uint64_t scaled =
		(static_cast<uint64_t>(p3) * fixed_point::kQ15One + (kPotCubeMax / 2)) / kPotCubeMax;
	return static_cast<uint16_t>(
		scaled > fixed_point::kQ15One ? fixed_point::kQ15One : scaled);
}

uint16_t compute_coeff_q15(uint32_t dt_us, uint16_t rate_q15) {
	if (rate_q15 == 0) return fixed_point::kQ15One;
	const uint64_t denominator =
		(static_cast<uint64_t>(rate_q15) * kMaxSlewUs) / fixed_point::kQ15One;
	if (denominator <= kMinSlewDenominatorUs) return fixed_point::kQ15One;
	const uint64_t numerator = static_cast<uint64_t>(dt_us) * fixed_point::kQ15One;
	const uint64_t scaled = (numerator + (denominator / 2)) / denominator;
	return static_cast<uint16_t>(
		scaled >= fixed_point::kQ15One ? fixed_point::kQ15One : scaled);
}

int32_t slew_channel_mv(int32_t input_mv, int32_t current_mv, uint16_t rise_coeff_q15,
						uint16_t fall_coeff_q15, uint16_t shape_q15) {
	const int32_t diff_mv = input_mv - current_mv;
	if (diff_mv == 0) return current_mv;

	const uint16_t coeff_q15 = diff_mv > 0 ? rise_coeff_q15 : fall_coeff_q15;
	const int32_t exp_step_mv = fixed_point::mul_q15(diff_mv, coeff_q15);

	int32_t linear_move_mv = fixed_point::mul_q15(kMaxMillivolts, coeff_q15);
	if (diff_mv < 0) linear_move_mv = -linear_move_mv;
	if ((diff_mv > 0 && linear_move_mv > diff_mv) || (diff_mv < 0 && linear_move_mv < diff_mv)) {
		linear_move_mv = diff_mv;
	}

	const int32_t step_mv = fixed_point::blend_q15(linear_move_mv, exp_step_mv, shape_q15);
	if (step_mv == 0) {
		return current_mv + (diff_mv > 0 ? 1 : -1);
	}
	return current_mv + step_mv;
}
}  // namespace

int main() {
	// Pot mapping and coeffs should stay bounded.
	assert(pot_to_slew_rate_q15(0) == 0);
	assert(pot_to_slew_rate_q15(255) == fixed_point::kQ15One);
	assert(compute_coeff_q15(1000, 0) == fixed_point::kQ15One);
	assert(compute_coeff_q15(100000, fixed_point::kQ15One) <= fixed_point::kQ15One);

	// Linear branch never overshoots target.
	const int32_t linear_step = slew_channel_mv(5000, 1000, 3000, 3000, 0) - 1000;
	assert(linear_step > 0);
	assert(linear_step <= (5000 - 1000));

	// Exponential branch moves proportionally and with correct sign.
	const int32_t exp_up =
		slew_channel_mv(8000, 2000, 16384, 16384, fixed_point::kQ15One) - 2000;
	const int32_t exp_down =
		slew_channel_mv(1000, 7000, 16384, 16384, fixed_point::kQ15One) - 7000;
	assert(exp_up > 0);
	assert(exp_down < 0);

	// Shape extremes select expected behavior.
	const int32_t s_linear = slew_channel_mv(9000, 0, 8192, 8192, 0);
	const int32_t s_exp = slew_channel_mv(9000, 0, 8192, 8192, fixed_point::kQ15One);
	assert(s_linear != s_exp);

	// Full exponential with tiny coeff should still eventually reach target (no deadband lock).
	int32_t current = 0;
	for (int i = 0; i < 2000; ++i) {
		current = slew_channel_mv(100, current, 50, 50, fixed_point::kQ15One);
	}
	assert(current == 100);

	// Bipolar signal zero should map to 5V center in DAC domain.
	assert(fixed_point::clamp_i32(0 + kCenterMillivolts, 0, kMaxMillivolts) == 5000);

	std::puts("slew_limiter_math_test: PASS");
	return 0;
}
