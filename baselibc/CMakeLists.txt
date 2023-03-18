cmake_minimum_required(VERSION 3.13)
include(toolchain-arm-none-eabi.cmake)

project(c C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(TINYLIBC_SRC_DIR ${TINYLIBC_PATH}/src)
set(TINYLIBC_INC_DIR ${TINYLIBC_PATH}/include)
add_library(
	c
	${TINYLIBC_SRC_DIR}/asprintf.c
	${TINYLIBC_SRC_DIR}/atoi.c
	${TINYLIBC_SRC_DIR}/atol.c
	${TINYLIBC_SRC_DIR}/atoll.c
	${TINYLIBC_SRC_DIR}/bsearch.c
	${TINYLIBC_SRC_DIR}/bzero.c
	${TINYLIBC_SRC_DIR}/calloc.c
	${TINYLIBC_SRC_DIR}/fgets.c
	${TINYLIBC_SRC_DIR}/inline.c
	${TINYLIBC_SRC_DIR}/jrand48.c
	${TINYLIBC_SRC_DIR}/lrand48.c
	${TINYLIBC_SRC_DIR}/malloc.c
	${TINYLIBC_SRC_DIR}/memccpy.c
	${TINYLIBC_SRC_DIR}/memchr.c
	${TINYLIBC_SRC_DIR}/memcmp.c
	${TINYLIBC_SRC_DIR}/memcpy.c
	${TINYLIBC_SRC_DIR}/memfile.c
	${TINYLIBC_SRC_DIR}/memmem.c
	${TINYLIBC_SRC_DIR}/memmove.c
	${TINYLIBC_SRC_DIR}/memrchr.c
	${TINYLIBC_SRC_DIR}/memset.c
	${TINYLIBC_SRC_DIR}/memswap.c
	${TINYLIBC_SRC_DIR}/mrand48.c
	${TINYLIBC_SRC_DIR}/nrand48.c
	${TINYLIBC_SRC_DIR}/qsort.c
	${TINYLIBC_SRC_DIR}/realloc.c
	${TINYLIBC_SRC_DIR}/sprintf.c
	${TINYLIBC_SRC_DIR}/srand48.c
	${TINYLIBC_SRC_DIR}/sscanf.c
	${TINYLIBC_SRC_DIR}/strcasecmp.c
	${TINYLIBC_SRC_DIR}/strcat.c
	${TINYLIBC_SRC_DIR}/strchr.c
	${TINYLIBC_SRC_DIR}/strcmp.c
	${TINYLIBC_SRC_DIR}/strcpy.c
	${TINYLIBC_SRC_DIR}/strcspn.c
	${TINYLIBC_SRC_DIR}/strdup.c
	${TINYLIBC_SRC_DIR}/strlcat.c
	${TINYLIBC_SRC_DIR}/strlcpy.c
	${TINYLIBC_SRC_DIR}/strlen.c
	${TINYLIBC_SRC_DIR}/strncasecmp.c
	${TINYLIBC_SRC_DIR}/strncat.c
	${TINYLIBC_SRC_DIR}/strncmp.c
	${TINYLIBC_SRC_DIR}/strncpy.c
	${TINYLIBC_SRC_DIR}/strndup.c
	${TINYLIBC_SRC_DIR}/strnlen.c
	${TINYLIBC_SRC_DIR}/strntoimax.c
	${TINYLIBC_SRC_DIR}/strntoumax.c
	${TINYLIBC_SRC_DIR}/strpbrk.c
	${TINYLIBC_SRC_DIR}/strrchr.c
	${TINYLIBC_SRC_DIR}/strsep.c
	${TINYLIBC_SRC_DIR}/strspn.c
	${TINYLIBC_SRC_DIR}/strstr.c
	${TINYLIBC_SRC_DIR}/strtoimax.c
	${TINYLIBC_SRC_DIR}/strtok.c
	${TINYLIBC_SRC_DIR}/strtok_r.c
	${TINYLIBC_SRC_DIR}/strtol.c
	${TINYLIBC_SRC_DIR}/strtoll.c
	${TINYLIBC_SRC_DIR}/strtoul.c
	${TINYLIBC_SRC_DIR}/strtoull.c
	${TINYLIBC_SRC_DIR}/strtoumax.c
	#${TINYLIBC_SRC_DIR}/tinyprintf.c
	#${TINYLIBC_SRC_DIR}/vasprintf.c
	#${TINYLIBC_SRC_DIR}/vprintf.c
	#${TINYLIBC_SRC_DIR}/vsprintf.c
	#${TINYLIBC_SRC_DIR}/vsscanf.c
)

message(STATUS "TINYLIBC_SRC_DIR: ${TINYLIBC_SRC_DIR}")
message(STATUS "TINYLIBC_INC_DIR: ${TINYLIBC_INC_DIR}")

target_include_directories(
	c
	PRIVATE
	.
	${TINYLIBC_INC_DIR}
	${TINYLIBC_SRC_DIR}/templates
)
