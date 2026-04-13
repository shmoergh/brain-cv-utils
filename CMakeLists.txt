file(GLOB FILES src/*.cpp src/*.h)
cmake_minimum_required(VERSION 3.22)

# Default board/platform. Can be overridden at configure time with:
#   -DPICO_BOARD=<board>
#   -DPICO_PLATFORM=<platform>
if(NOT DEFINED PICO_BOARD)
	set(PICO_BOARD pico2)
endif()

if(NOT DEFINED PICO_PLATFORM)
	set(PICO_PLATFORM rp2350-arm-s)
endif()

# Include Pico SDK (from brain-sdk subdirectory)
include(brain-sdk/pico_sdk_import.cmake)

project(brain-cv-utils C CXX ASM)

# Reserve top-of-flash sectors for Brain SDK storage before Pico SDK init.
set(BRAIN_STORAGE_ENABLE_FLASH_RESERVATION ON CACHE BOOL "" FORCE)
set(BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT OFF CACHE BOOL "" FORCE)
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()

# Initialize the Pico SDK
pico_sdk_init()

# Add brain-sdk which includes brain libraries
add_subdirectory(brain-sdk)

add_executable(brain-cv-utils ${FILES})

target_link_libraries(brain-cv-utils
	pico_stdlib
	brain)

target_compile_definitions(brain-cv-utils PRIVATE
	BRAIN_USE_BUTTONS=1
	BRAIN_USE_LEDS=1
	BRAIN_USE_STORAGE=1
	BRAIN_USE_OUTPUTS=1
	BRAIN_USE_INPUTS=1
	BRAIN_USE_POTS=1
)

pico_enable_stdio_usb(brain-cv-utils 1)
pico_enable_stdio_uart(brain-cv-utils 1)
pico_add_extra_outputs(brain-cv-utils)
