# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(noza_os_demo C CXX ASM)

#list(APPEND PICO_CONFIG_HEADER_FILES ${PROJECT_SOURCE_DIR}/piccolo_os_lock_core_v1.1.h)

# Initialise the Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(
	noza_os_demo
	kernel/context_switch.S
	kernel/noza_syscall.S
	kernel/noza_os.c
	kernel/noza_os.h
	user/syslib.c
	user/noza_os_demo.c
)

target_include_directories(noza_os_demo PRIVATE .)

pico_set_program_name(noza_os_demo "noza_os_demo")
pico_set_program_version(noza_os_demo "0.1.1")

pico_enable_stdio_uart(noza_os_demo 1)
pico_enable_stdio_usb(noza_os_demo 0)

# Add the standard library to the build
target_link_libraries(noza_os_demo pico_stdlib pico_multicore)

pico_add_extra_outputs(noza_os_demo)
pico_enable_stdio_usb(noza_os_demo 1)
pico_enable_stdio_uart(noza_os_demo 1)

