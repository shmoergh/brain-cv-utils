#ifndef VOLTAGE_SMOOTHER_H_
#define VOLTAGE_SMOOTHER_H_

#include <cstdint>

#include "fixed-point.h"

class VoltageSmoother {
public:
	constexpr VoltageSmoother(int32_t deadband_mv, uint16_t alpha_q15)
		: deadband_mv_(deadband_mv), alpha_q15_(alpha_q15) {}

	void reset(int32_t value_mv) {
		last_output_mv_ = value_mv;
		initialized_ = true;
	}

	int32_t process(int32_t target_mv) {
		if (!initialized_) {
			last_output_mv_ = target_mv;
			initialized_ = true;
			return last_output_mv_;
		}

		const int32_t diff_mv = target_mv - last_output_mv_;
		const int32_t abs_diff_mv = diff_mv < 0 ? -diff_mv : diff_mv;
		if (abs_diff_mv <= deadband_mv_) {
			return last_output_mv_;
		}

		int32_t step_mv = fixed_point::mul_q15(diff_mv, alpha_q15_);
		if (step_mv == 0) {
			step_mv = diff_mv > 0 ? 1 : -1;
		}

		last_output_mv_ += step_mv;
		return last_output_mv_;
	}

private:
	int32_t deadband_mv_;
	uint16_t alpha_q15_;
	int32_t last_output_mv_ = 0;
	bool initialized_ = false;
};

#endif  // VOLTAGE_SMOOTHER_H_
