cmake_minimum_required(VERSION 3.13)
include(toolchain-host.cmake)

project(romfs_gen C CXX ASM)

set(CMAKE_C_STANDARD	11)
set(CMAKE_CXX_STANDARD	17)

add_executable(
	romfs_gen
	${CMAKE_CURRENT_LIST_DIR}/romfs_util.c
)
