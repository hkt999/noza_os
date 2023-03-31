cmake_minimum_required(VERSION 3.12)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(HOST_PLATFORM "rtos")
set(CMAKE_VERBOSE_MAKEFILE OFF)

project(tflm-pico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-as)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_RANLIB arm-none-eabi-ranlib)

set(CMAKE_C_FLAGS "-mcpu=cortex-m0plus -march=armv6-m -mthumb -O3 \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -DCMSIS_NN -DPROJECT_GENERATION")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m0plus -march=armv6-m -mthumb -O3 \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -fno-threadsafe-statics -DCMSIS_NN -DPROJECT_GENERATION")

set(LUA_PATH ${CMAKE_CURRENT_LIST_DIR}/../lua)
set(NOZAOS_PATH ${CMAKE_CURRENT_LIST_DIR}/../..)
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
    lua_noza.c # main program
)

target_include_directories(
    lua
    PRIVATE
    ${NOZAOS_PATH}/user/console
	${CMAKE_CURRENT_LIST_DIR}
    ${LUA_PATH}
)
