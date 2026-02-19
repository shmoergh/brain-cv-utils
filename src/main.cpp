#include <pico/stdlib.h>
#include <stdio.h>

#include "cv-utils.h"

int main() {
	stdio_init_all();

	CvUtils cv_utils;
	cv_utils.init();

	while (true) {
		cv_utils.update();
	}

	return 0;
}
