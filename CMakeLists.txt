cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)
project(noza_os C CXX ASM)

# initialise the Pico SDK
pico_sdk_init()

# declare the project apps
set(NOZAOS_UNITTEST					1)
set(NOZAOS_LUA						0)
set(NOZAOS_MUTEX					1)
set(NOZAOS_PTHREAD					1)

# tensorflow lite micro
set(NOZAOS_TFLM						0)
set(NOZAOS_TFLM_HELLO 				0)
set(NOZAOS_TFLM_MAGIC_WAND 			0)
set(NOZAOS_TFLM_PERSON_DETECTION	0)
set(NOZAOS_TFLM_MICRO_SPEECH		0)

add_definitions(-DPICO_USE_MALLOC_MUTEX=1)
# define sub module built and test

if (NOZAOS_UNITTEST)
	set(NOZAOS_UNITY_LIB "unity")
	add_definitions(-DNOZAOS_UNITTEST)
	add_subdirectory(3rd_party/unity_port)
endif()

if (NOZAOS_LUA)
	set(NOZAOS_LUA_LIB "lua")
	add_definitions(-DNOZAOS_LUA)
	add_subdirectory(3rd_party/lua_port)
endif()

if (NOZAOS_TFLM)
	add_definitions(-DNOZAOS_TFLM)
	add_subdirectory(3rd_party/tflm-cmsis)
endif()

if (NOZAOS_TFLM_HELLO)
	set(NOZAOS_TFLM_HELLO_LIB "hello_world")
	add_definitions(-DNOZAOS_TFLM_HELLO)
endif()

if (NOZAOS_TFLM_MAGIC_WAND)
	set(NOZAOS_TFLM_MAGIC_WAND_LIB "magic_wand")
	add_definitions(-DNOZAOS_TFLM_MAGIC_WAND)
endif()

if (NOZAOS_TFLM_MICRO_SPEECH)
	set(NOZAOS_TFLM_MICRO_SPEECH_LIB "micro_speech")
	add_definitions(-DNOZAOS_TFLM_MICRO_SPEECH)
endif()

if (NOZAOS_TFLM_PERSON_DETECTION)
	set(NOZAOS_TFLM_MAGIC_WAND_LIB "person_detection")
	add_definitions(-DNOZAOS_TFLM_PERSON_DETECTION)
endif()

# add sub module built

# the noza kernel and user space application
add_executable(
	noza
	kernel/platform/rp2040_context_switch.S
	kernel/platform/rp2040.c
	kernel/platform/rp2040_build_stack.c
	kernel/noza_os.c

	# lib and system call for user space application
	user/libc/src/noza_syscall.S
	user/libc/src/app_run.S
	user/libc/src/syscall_asm.S
	user/libc/src/syslib.c
	user/libc/src/setjmp.S

	# services
	service/name_lookup/name_lookup.c
	service/mutex/mutex_serv.c
	service/mutex/mutex_client.c
	#service/memory/mem_serv.c
	#service/vfs/rootfs.c
	#service/vfs/romfs.c

	# applciation demo
	user/noza_os_demo.c

	# application console
	user/console/history.c
	user/console/cmd_line.c
	user/console/noza_console.c
	user/console/noza_term.c

	# unit test
	user/unit_test.c

	# pthread
	user/pthread/pthread.c
)

target_include_directories(
	noza
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/service/vfs
	${CMAKE_CURRENT_LIST_DIR}/include
	${CMAKE_CURRENT_LIST_DIR}/user/libc/include
	${CMAKE_CURRENT_LIST_DIR}/3rd_party
	${CMAKE_CURRENT_LIST_DIR}/3rd_party/Unity/src
)

# Add the standard library to the build
target_link_libraries(
	noza
	pico_stdlib
	pico_multicore
	${NOZAOS_UNITY_LIB}
	${NOZAOS_LUA_LIB}
	${NOZAOS_TFLM_HELLO_LIB}
	${NOZAOS_TFLM_MAGIC_WAND_LIB}
	${NOZAOS_TFLM_MICRO_SPEECH_LIB}
	${NOZAOS_TFLM_PERSON_DETECTION_LIB}
)

pico_add_extra_outputs(noza)
pico_enable_stdio_usb(noza 1)

