cmake_minimum_required(VERSION 3.12)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(HOST_PLATFORM "rtos")
set(CMAKE_VERBOSE_MAKEFILE OFF)

project(noza_posix C CXX ASM)

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
	-fno-asynchronous-unwind-tables -DPROJECT_GENERATION")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m0plus -march=armv6-m -mthumb -O3 \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -fno-threadsafe-statics -DPROJECT_GENERATION")

add_library(
	pthread
	pthread_attr.c
	pthread_primitives.c
	pthread_mutex.c
	pthread_mutex_attr.c
	pthread_cond.c
	pthread_cond_attr.c
	pthread_sched.c
	pthread_not_supported.c
	semaphore.c
	noza_time.c
)

target_include_directories(
	pthread
    PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/../..
	${CMAKE_CURRENT_LIST_DIR}/../../user/libc/include
)
