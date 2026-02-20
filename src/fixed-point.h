#ifndef FIXED_POINT_H_
#define FIXED_POINT_H_

#include <cstdint>

namespace fixed_point {

constexpr int32_t kQ15One = 32768;

inline constexpr int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

inline constexpr int32_t mul_q15(int32_t value, uint16_t gain_q15) {
	return static_cast<int32_t>((static_cast<int64_t>(value) * gain_q15) / kQ15One);
}

inline constexpr int32_t blend_q15(int32_t a, int32_t b, uint16_t t_q15) {
	return static_cast<int32_t>(
		((static_cast<int64_t>(a) * (kQ15One - t_q15)) + (static_cast<int64_t>(b) * t_q15)) /
		kQ15One);
}

inline constexpr uint16_t u8_to_q15(uint8_t v) {
	constexpr uint32_t kU8Max = 255;
	const uint32_t scaled = (static_cast<uint32_t>(v) * kQ15One + (kU8Max / 2)) / kU8Max;
	return static_cast<uint16_t>(scaled > kQ15One ? kQ15One : scaled);
}

}  // namespace fixed_point

#endif  // FIXED_POINT_H_
