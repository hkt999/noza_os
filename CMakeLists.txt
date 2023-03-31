# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
add_definitions(-DPICO_USE_MALLOC_MUTEX=1)
add_definitions(-DNOZAOS_UNITTEST)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(noza_os_demo C CXX ASM)

# Initialise the Pico SDK
pico_sdk_init()

#add_subdirectory(3rd_party/tflm-cmsis)
add_subdirectory(3rd_party/fatfs_port)

set(LUA_PATH ${CMAKE_CURRENT_LIST_DIR}/3rd_party/lua)
set(LUA_PORT_PATH ${CMAKE_CURRENT_LIST_DIR}/3rd_party/lua_port)
#add_compile_definitions(lua LUA_USE_POSIX)
add_library(
    lua
    ${LUA_PATH}/lapi.c
    ${LUA_PATH}/lauxlib.c
    ${LUA_PATH}/lbaselib.c
    ${LUA_PATH}/lcode.c
    ${LUA_PATH}/lcorolib.c
    ${LUA_PATH}/lctype.c
    ${LUA_PATH}/ldblib.c
    ${LUA_PATH}/ldebug.c
    ${LUA_PATH}/ldo.c
    ${LUA_PATH}/ldump.c
    ${LUA_PATH}/lfunc.c
    ${LUA_PATH}/lgc.c
    ${LUA_PATH}/linit.c
    ${LUA_PATH}/liolib.c
    ${LUA_PATH}/llex.c
    ${LUA_PATH}/lmathlib.c
    ${LUA_PATH}/lmem.c
    ${LUA_PATH}/loadlib.c
    ${LUA_PATH}/lobject.c
    ${LUA_PATH}/lopcodes.c
    ${LUA_PATH}/loslib.c
    ${LUA_PATH}/lparser.c
    ${LUA_PATH}/lstate.c
    ${LUA_PATH}/lstring.c
    ${LUA_PATH}/lstrlib.c
    ${LUA_PATH}/ltable.c
    ${LUA_PATH}/ltablib.c
    ${LUA_PATH}/ltests.c
    ${LUA_PATH}/ltm.c
    ${LUA_PATH}/lundump.c
    ${LUA_PATH}/lutf8lib.c
    ${LUA_PATH}/lvm.c
    ${LUA_PATH}/lzio.c
    ${LUA_PORT_PATH}/lua_noza.c # main program
)

target_include_directories(
	lua
	PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/user/console
	${LUA_PORT_PATH}
	${LUA_PATH}
)

add_executable(
	noza
	kernel/context_switch.S
	kernel/noza_os.c

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

pico_set_program_name(noza "noza")
pico_set_program_version(noza "0.1.1")

# Add the standard library to the build
target_link_libraries(
	noza
	pico_stdlib
	pico_multicore
	lua
#    hello_world
#    magic_wand
#    person_detection
#    micro_speech
)

pico_add_extra_outputs(noza)
pico_enable_stdio_usb(noza 1)
#pico_enable_stdio_uart(noza 1)

