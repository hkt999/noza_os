cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# declare the project apps
option(NOZAOS_UNITTEST "build unit test" ON)

# posix system
option(NOZAOS_POSIX	"build posix" ON)
option(NOZAOS_UNITTEST_POSIX "build posix unit test" ON)

# lua interpreter
option(NOZAOS_LUA "build lua interpreter" OFF)

# tensorflow lite micro
option(NOZAOS_TFLM "build tensorflow lite micro" OFF)
option(NOZAOS_TFLM_HELLO_WORLD "build tensorflow lite micro hello world" OFF)
option(NOZAOS_TFLM_MAGIC_WAND "build tensorflow lite micro magic wand" OFF)
option(NOZAOS_TFLM_PERSON_DETECTION "build tensorflow lite micro person detection" OFF)
option(NOZAOS_TFLM_MICRO_SPEECH "build tensorflow lite micro micro speech" OFF)

option(NOZAOS_DRIVER_WS2812 "build ws2812 LED controll driver" OFF)

if (NOZAOS_UNITTEST_POSIX)
	message(STATUS "NOZAOS_UNITTEST_POSIX need NOZAOS_POSIX")
	set(NOZAOS_POSIX ON cache bool "Automatically set because of NOZAOS_UNITTEST_POSIX" force )
endif()

if (NOZAOS_TFLM_HELLO_WORLD AND NOT NOZAOS_TFLM)
	message(STATUS "NOZAOS_TFLM_HELLO_WORLD need NOZAOS_TFLM")
	set(NOZAOS_TFLM ON cache bool "Automatically set because of NOZAOS_TFLM_HELLO_WORLD" force )
endif()

if (NOZAOS_TFLM_MAGIC_WAND AND NOT NOZAOS_TFLM)
	message(STATUS "NOZAOS_TFLM_MAGIC_WAND need NOZAOS_TFLM")
	set(NOZAOS_TFLM ON cache bool "Automatically set because of NOZAOS_TFLM_MAGIC_WAND" force )
endif()

if (NOZAOS_TFLM_PERSON_DETECTION AND NOT NOZAOS_TFLM)
	message(STATUS "NOZAOS_TFLM_PERSON_DETECTION need NOZAOS_TFLM")
	set(NOZAOS_TFLM ON cache bool "Automatically set because of NOZAOS_TFLM_PERSON_DETECTION" force )
endif()

if (NOZAOS_TFLM_MICRO_SPEECH AND NOT NOZAOS_TFLM)
	message(STATUS "NOZAOS_TFLM_MICRO_SPEECH need NOZAOS_TFLM")
	set(NOZAOS_TFLM ON cache bool "Automatically set because of NOZAOS_TFLM_MICRO_SPEECH" force )
endif()

set(TFLM_OPTIONS 
    ${NOZAOS_TFLM_HELLO_WORLD}
    ${NOZAOS_TFLM_MAGIC_WAND}
    ${NOZAOS_TFLM_PERSON_DETECTION}
    ${NOZAOS_TFLM_MICRO_SPEECH}
)

set(TFLM_DEMO_COUNT 0)
foreach(OPT IN LISTS TFLM_OPTIONS)
    if(OPT)
        math(EXPR TFLM_DEMO_COUNT "${TFLM_DEMO_COUNT} + 1")
    endif()
endforeach()

if(TFLM_DEMO_COUNT GREATER 1)
    message(FATAL_ERROR "*** Only one tensorflow lite micro demo can be built ***")
endif()


# not showing option in UI
mark_as_advanced(
	CMAKE_BACKWARDS_COMPATIBILITY
	CMAKE_INSTALL_PREFIX
	CMAKE_TOOLCHAIN_FILE
	EXECUTABLE_OUTPUT_PATH
	LIBRARY_OUTPUT_PATH
	PICO_BOARD
	PICO_BOARD_HEADER_DIRS
	PICO_BUILD_DOCS
	PICO_COMPILER_CC
	PICO_COMPILER_CXX
	PICO_COPY_TO_RAM
	PICO_DEFAULT_BOOT_STAGE2
	PICO_DEOPTIMIZED_DEBUG
	PICO_NO_FLASH
	PICO_OBJCOPY
	PICO_OBJDUMP
	PICO_PLATFORM
	PICO_SDK_FETCH_FROM_GIT
	PICO_SDK_FETCH_FROM_GIT_PATH
	PICO_SDK_PATH
	PICO_STDIO_SEMIHOSTING
	pico-sdk-tools_DIR
)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)
project(noza_os C CXX ASM)

# initialise the Pico SDK
pico_sdk_init()

add_definitions(-DPICO_USE_MALLOC_MUTEX=1)
# define sub module built and test

if (NOZAOS_UNITTEST OR NOZAOS_UNITTEST_POSIX)
	set(NOZAOS_UNITY_LIB "unity")
	add_subdirectory(3rd_party/unity_port)
endif()

if (NOZAOS_POSIX)
	set(NOZAOS_POSIX_LIB "pthread")
	add_subdirectory(user/posix)
endif()

if (NOZAOS_LUA)
	set(NOZAOS_LUA_LIB "lua")
	add_subdirectory(3rd_party/lua_port)
endif()

if (NOZAOS_TFLM)
	set(NOZAOS_TFLM_LIB "tflm_cmsis")
	add_subdirectory(3rd_party/tflm-cmsis)
endif()

if (NOZAOS_TFLM_HELLO_WORLD)
	set(NOZAOS_TFLM_HELLO_LIB "hello_world")
endif()

if (NOZAOS_TFLM_MAGIC_WAND)
	set(NOZAOS_TFLM_MAGIC_WAND_LIB "magic_wand")
endif()

if (NOZAOS_TFLM_MICRO_SPEECH)
	set(NOZAOS_TFLM_MICRO_SPEECH_LIB "micro_speech")
endif()

if (NOZAOS_TFLM_PERSON_DETECTION)
	set(NOZAOS_TFLM_MAGIC_WAND_LIB "person_detection")
endif()

# add sub module built

# the noza kernel and user space application
set(
	NOZA_SOURCES
	kernel/platform/rp2040_context_switch.S
	kernel/platform/rp2040_platform.c
	kernel/platform/rp2040_build_stack.c
	kernel/noza_os.c

	# lib and system call for user space application
	user/libc/src/app_run.S
	user/libc/src/syscall_asm.S
	user/libc/src/syslib.c
	user/libc/src/proc_api.c
	user/libc/src/thread_api.c
	user/libc/src/setjmp.S
	user/libc/src/spinlock.c
	user/libc/src/nz_stdlib.c

	# services
	service/name_lookup/name_lookup_server.c
	service/name_lookup/name_lookup_client.c
	service/name_lookup/string_map.c

	service/memory/mem_serv.c
	service/memory/mem_client.c
	service/sync/sync_serv.c
	service/sync/sync_client.c
	service/vfs/rootfs.c
	service/vfs/romfs.c
	type/dblist.c
	type/hashslot.c
	3rd_party/tinyalloc_port/tinyalloc.c

	# applciation demo
	user/noza_os_demo.c

	# application console
	user/console/history.c
	user/console/cmd_line.c
	user/console/noza_console.c
	user/console/noza_term.c
)

if (NOZAOS_UNITTEST) 
	list(APPEND NOZA_SOURCES user/noza_unit_test/noza_unit_test.c)
endif()

if (NOZAOS_UNITTEST_POSIX)
	list(APPEND NOZA_SOURCES user/posix_unit_test/posix_unit_test.c)
endif()

if (NOZAOS_LUA)
	list(APPEND NOZA_SOURCES 3rd_party/lua_port/lua_noza.c)
endif()

if (NOZAOS_TFLM)
	list(APPEND NOZA_SOURCES 3rd_party/tflm_port/tflm_port.c)
endif()

if (NOZAOS_TFLM_HELLO_WORLD)
	list(APPEND NOZA_SOURCES 3rd_party/tflm_port/noza_hello_world.c)
endif()

if (NOZAOS_TFLM_MAGIC_WAND)
	list(APPEND NOZA_SOURCES 3rd_party/tflm_port/noza_magic_wand.c)
endif()

if (NOZAOS_TFLM_MICRO_SPEECH)
	list(APPEND NOZA_SOURCES 3rd_party/tflm_port/noza_micro_speech.c)
endif()

if (NOZAOS_TFLM_PERSON_DETECTION)
	list(APPEND NOZA_SOURCES 3rd_party/tflm_port/noza_person_detection.c)
endif()

if (NOZAOS_DRIVER_WS2812)
	list(APPEND NOZA_SOURCES drivers/ws2812/ws2812.c)
	list(APPEND NOZA_SOURCES drivers/ws2812/example.c)
endif()

add_executable(
	noza
	${NOZA_SOURCES}
)
if (NOZAOS_DRIVER_WS2812)
	pico_generate_pio_header( # generate pico header for ws2812.pio module
		noza
		${CMAKE_CURRENT_LIST_DIR}/drivers/ws2812/ws2812.pio
	)
endif()

set(
	NOZA_INCLUDES
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/include
	${CMAKE_CURRENT_LIST_DIR}/user/libc/include
	${CMAKE_CURRENT_LIST_DIR}/user
	${CMAKE_CURRENT_LIST_DIR}/3rd_party
)

if (NOZAOS_LUA)
	list(APPEND NOZA_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/3rd_party/lua)
endif()

if (NOZAOS_UNITTEST OR NOZAOS_UNITTEST_POSIX)
	list(APPEND NOZA_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/3rd_party/unity_port)
endif()

target_include_directories(
	noza
	PRIVATE
	${NOZA_INCLUDES}
	${CMAKE_CURRENT_LIST_DIR}/user/console
)

# Add the standard library to the build

set(
	NOZA_LIBS
	pico_stdlib
	pico_multicore
	${NOZAOS_POSIX_LIB}
	${NOZAOS_UNITY_LIB}
)

# Libraries
if (NOZAOS_LUA)
	list(APPEND NOZA_LIBS ${NOZAOS_LUA_LIB})
endif()

if (NOZAOS_TFLM_HELLO_WORLD)
	list(APPEND NOZA_LIBS ${NOZAOS_TFLM_HELLO_LIB})
endif()

if (NOZAOS_TFLM_MAGIC_WAND)
	list(APPEND NOZA_LIBS ${NOZAOS_TFLM_MAGIC_WAND_LIB})
endif()

if (NOZAOS_TFLM_MICRO_SPEECH)
	list(APPEND NOZA_LIBS ${NOZAOS_TFLM_MICRO_SPEECH_LIB})
endif()

if (NOZAOS_TFLM_PERSON_DETECTION)
	list(APPEND NOZA_LIBS ${NOZAOS_TFLM_PERSON_DETECTION_LIB})
endif()

if (NOZAOS_TFLM)
	list(APPEND NOZA_LIBS ${NOZAOS_TFLM_LIB})
endif()

if (NOZAOS_DRIVER_WS2812)
	list(APPEND NOZA_LIBS hardware_pio)
endif()

target_link_libraries(
	noza
	pico_stdlib
	pico_multicore
	${NOZA_LIBS}
)

pico_add_extra_outputs(noza)
pico_enable_stdio_usb(noza 1)

