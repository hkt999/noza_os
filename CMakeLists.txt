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

# Initialise the Pico SDK
pico_sdk_init()

add_executable(
	noza
	kernel/context_switch.S
	kernel/noza_syscall.S
	kernel/noza_os.c
	kernel/noza_os.h
	user/syslib.c
	user/noza_os_demo.c
	service/vfs/rootfs.c
	service/vfs/romfs.c
	3rd_party/rpmalloc/rpmalloc.c
)

target_include_directories(
	noza
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/service/vfs
	${CMAKE_CURRENT_LIST_DIR}/include
)

pico_set_program_name(noza "noza")
pico_set_program_version(noza "0.1.1")

pico_enable_stdio_uart(noza 1)
pico_enable_stdio_usb(noza 0)

# Add the standard library to the build
target_link_libraries(noza pico_stdlib pico_multicore)

pico_add_extra_outputs(noza)
pico_enable_stdio_usb(noza 1)
pico_enable_stdio_uart(noza 1)

