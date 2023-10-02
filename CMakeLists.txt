cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)
project(noza_os C CXX ASM)
#project(noza_kernel C CXX ASM)

# initialise the Pico SDK
pico_sdk_init()

add_definitions(-DPICO_USE_MALLOC_MUTEX=1)
# define sub module built and test
# add_definitions(-DNOZAOS_TFCMSIS)
# add_definitions(-DNOZAOS_LUA)
# add_definitions(-DNOZAOS_UNITTEST)

# add sub module built
# add_subdirectory(3rd_party/tflm-cmsis)
# add_subdirectory(3rd_party/fatfs_port)
# add_subdirectory(3rd_party/lua_port)

# the noza kernel and user space application
add_executable(
	noza
	kernel/context_switch.S
	kernel/noza_os.c
	kernel/platform.c

	# lib and system call for user space application
	user/libc/src/noza_syscall.S
	user/libc/src/app_bootstrap.S
	user/libc/src/syscall_asm.S
	user/libc/src/syslib.c
	user/libc/src/setjmp.S

	# applciation demo
	user/noza_os_demo.c

	# application console
	user/console/history.c
	user/console/cmd_line.c
	user/console/noza_console.c
	user/console/noza_term.c

	service/name_lookup/name_lookup.c
	service/memory/mem_serv.c
	service/vfs/rootfs.c
	service/vfs/romfs.c

	# unit test
	user/unit_test.c
)

target_include_directories(
	noza
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/service/vfs
	${CMAKE_CURRENT_LIST_DIR}/include
	${CMAKE_CURRENT_LIST_DIR}/user/libc/include
	${CMAKE_CURRENT_LIST_DIR}/3rd_party
)

# Add the standard library to the build
target_link_libraries(
	noza
	pico_stdlib
	pico_multicore
#	lua
#    hello_world
#    magic_wand
#    person_detection
#    micro_speech
)

pico_add_extra_outputs(noza)
pico_enable_stdio_usb(noza 1)
#pico_enable_stdio_uart(noza 1)

