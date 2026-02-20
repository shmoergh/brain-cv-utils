#include <cassert>
#include <cstdio>

#include "../src/voltage-smoother.h"

int main() {
	// Deadband should hold small jitter around current value.
	VoltageSmoother hold_only(3, 32768);
	assert(hold_only.process(5000) == 5000);
	assert(hold_only.process(5002) == 5000);
	assert(hold_only.process(4998) == 5000);
	assert(hold_only.process(5004) == 5004);

	// With low alpha, output should still converge to target over time.
	VoltageSmoother smooth(0, 256);
	int32_t v = smooth.process(0);
	for (int i = 0; i < 1000; ++i) {
		v = smooth.process(1000);
	}
	assert(v == 1000);

	std::puts("voltage_smoother_test: PASS");
	return 0;
}
