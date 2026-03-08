# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(NOZA_PLATFORM "rp2040" CACHE STRING "Target platform")
set_property(CACHE NOZA_PLATFORM PROPERTY STRINGS rp2040 qemu_mps2_an385)

if (NOT NOZA_PLATFORM STREQUAL "rp2040" AND NOT NOZA_PLATFORM STREQUAL "qemu_mps2_an385")
    message(FATAL_ERROR "Unsupported NOZA_PLATFORM=${NOZA_PLATFORM}")
endif()

if (NOZA_PLATFORM STREQUAL "qemu_mps2_an385")
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain-arm-none-eabi.cmake)
endif()

option(NOZAOS_UNITTEST "build unit test" ON)
option(NOZAOS_STRICT_WARNINGS "enable stricter compiler warnings for project sources" OFF)
option(NOZAOS_POSIX "build posix" ON)
option(NOZAOS_UNITTEST_POSIX "build posix unit test" ON)
option(NOZA_PROCESS_USE_TLSF "use TLSF for per-process heap allocator" OFF)
option(NOZAOS_LUA "build lua interpreter" OFF)
option(NOZAOS_DRIVER_WS2812 "build ws2812 LED controll driver" OFF)

if (NOZAOS_UNITTEST_POSIX)
    message(STATUS "NOZAOS_UNITTEST_POSIX need NOZAOS_POSIX")
    set(NOZAOS_POSIX ON CACHE BOOL "Automatically set because of NOZAOS_UNITTEST_POSIX" FORCE)
endif()

if (NOZA_PLATFORM STREQUAL "qemu_mps2_an385" AND NOZAOS_DRIVER_WS2812)
    message(FATAL_ERROR "WS2812 driver is only supported on the RP2040 platform")
endif()

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

if (NOZA_PLATFORM STREQUAL "rp2040")
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    if(NOT PICO_SDK_PATH OR NOT EXISTS "${PICO_SDK_PATH}")
        set(PICO_SDK_PATH ${USERHOME}/.pico-sdk/sdk/${sdkVersion})
    endif()
    set(PICO_SDK_PATH ${PICO_SDK_PATH} CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)

    set(PICO_STDIO_USB OFF CACHE BOOL "Enable stdio over USB" FORCE)
    set(PICO_STDIO_UART ON CACHE BOOL "Enable stdio over UART" FORCE)

    include(pico_sdk_import.cmake)
endif()

project(noza_os C CXX ASM)

if (NOZA_PLATFORM STREQUAL "rp2040")
    pico_sdk_init()
    add_compile_definitions(NOZA_PLATFORM_RP2040=1)
else()
    add_compile_definitions(NOZA_PLATFORM_QEMU_MPS2_AN385=1)
endif()

if (NOZAOS_UNITTEST OR NOZAOS_UNITTEST_POSIX)
    set(NOZAOS_UNITY_LIB unity)
    add_subdirectory(3rd_party/unity_port)
endif()

if (NOZAOS_POSIX)
    set(NOZAOS_POSIX_LIB pthread)
    add_subdirectory(user/posix)
endif()

if (NOZAOS_LUA)
    set(NOZAOS_LUA_LIB lua)
    add_subdirectory(3rd_party/lua_port)
endif()

set(NOZA_ARCH_SOURCES
    kernel/arch/arm_m/context_switch.S
    kernel/arch/arm_m/build_stack.c
)

set(NOZA_CORE_SOURCES
    kernel/printk.c
    kernel/noza_uart.c
    kernel/noza_os.c

    user/libc/src/app_run.S
    user/libc/src/syscall_asm.S
    user/libc/src/syslib.c
    user/libc/src/proc_api.c
    user/libc/src/thread_api.c
    user/libc/src/setjmp.S
    user/libc/src/spinlock.c
    user/libc/src/nz_stdlib.c
    user/libc/src/syscall_io.c

    service/name_lookup/name_lookup_server.c
    service/name_lookup/name_lookup_client.c
    service/name_lookup/string_map.c

    service/memory/mem_serv.c
    service/memory/mem_client.c
    service/sync/sync_serv.c
    service/sync/sync_client.c
    drivers/uart/uart_io_service.c
    drivers/uart/uart_io_client.c
    drivers/uart/uart_devfs.c
    user/apps/shell/shell_main.c
    service/fs/fs_main.c
    service/fs/fs_dispatch.c
    service/fs/ramfs.c
    service/fs/vfs.c
    service/fs/devfs.c
    service/irq/irq_client.c
    service/vfs/rootfs.c
    service/vfs/romfs.c
    service/irq/irq_service.c
    service/app_launcher/app_launcher.c
    type/dblist.c
    type/hashslot.c
    3rd_party/tinyalloc_port/tinyalloc.c

    user/noza_os_demo.c

    user/console/history.c
    user/console/cmd_line.c
    user/console/noza_console.c
    user/console/noza_term.c
    user/libc/src/app_launcher_client.c
)

if (NOZA_PROCESS_USE_TLSF)
    list(APPEND NOZA_CORE_SOURCES 3rd_party/tlsf_port/tlsf.c)
endif()
if (NOZAOS_UNITTEST)
    list(APPEND NOZA_CORE_SOURCES user/noza_unit_test/noza_unit_test.c)
endif()
if (NOZAOS_UNITTEST_POSIX)
    list(APPEND NOZA_CORE_SOURCES user/posix_unit_test/posix_unit_test.c)
endif()
if (NOZAOS_LUA)
    list(APPEND NOZA_CORE_SOURCES 3rd_party/lua_port/lua_noza.c)
endif()
if (NOZAOS_DRIVER_WS2812)
    list(APPEND NOZA_CORE_SOURCES drivers/ws2812/ws2812.c drivers/ws2812/example.c)
endif()

if (NOZA_PLATFORM STREQUAL "rp2040")
    set(NOZA_PLATFORM_SOURCES
        kernel/platform/rp2040/platform.c
    )
    set(NOZA_PLATFORM_LIBS pico_stdlib pico_multicore)
    set(NOZA_PLATFORM_COMPILE_DEFS PICO_USE_MALLOC_MUTEX=1)
    set(NOZA_PLATFORM_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/kernel/platform/rp2040)
else()
    set(NOZA_PLATFORM_SOURCES
        kernel/platform/qemu_mps2_an385/startup.S
        kernel/platform/qemu_mps2_an385/platform.c
        kernel/platform/qemu_mps2_an385/init.c
    )
    set(NOZA_PLATFORM_LIBS c m gcc nosys)
    set(NOZA_PLATFORM_COMPILE_DEFS "")
    set(NOZA_PLATFORM_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/kernel/platform/qemu_mps2_an385)
endif()

add_executable(noza
    ${NOZA_ARCH_SOURCES}
    ${NOZA_CORE_SOURCES}
    ${NOZA_PLATFORM_SOURCES}
)

if (NOZA_PLATFORM STREQUAL "rp2040" AND NOZAOS_DRIVER_WS2812)
    pico_generate_pio_header(noza ${CMAKE_CURRENT_LIST_DIR}/drivers/ws2812/ws2812.pio)
endif()

set(NOZA_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/kernel
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/user/libc/include
    ${CMAKE_CURRENT_LIST_DIR}/user
    ${CMAKE_CURRENT_LIST_DIR}/3rd_party
    ${CMAKE_CURRENT_LIST_DIR}/kernel/arch
    ${NOZA_PLATFORM_INCLUDES}
)

if (NOZAOS_LUA)
    list(APPEND NOZA_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/3rd_party/lua)
endif()
if (NOZAOS_UNITTEST OR NOZAOS_UNITTEST_POSIX)
    list(APPEND NOZA_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/3rd_party/unity_port)
endif()
if (NOZA_PROCESS_USE_TLSF)
    list(APPEND NOZA_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/3rd_party/tlsf_port)
endif()

target_include_directories(noza PRIVATE ${NOZA_INCLUDES} ${CMAKE_CURRENT_LIST_DIR}/user/console)

target_link_libraries(noza PRIVATE ${NOZA_PLATFORM_LIBS} ${NOZAOS_POSIX_LIB} ${NOZAOS_UNITY_LIB})
if (NOZAOS_LUA)
    target_link_libraries(noza PRIVATE ${NOZAOS_LUA_LIB})
endif()
if (NOZA_PLATFORM STREQUAL "rp2040" AND NOZAOS_DRIVER_WS2812)
    target_link_libraries(noza PRIVATE hardware_pio)
endif()

target_compile_definitions(noza PRIVATE ${NOZA_PLATFORM_COMPILE_DEFS})
if (NOZA_PROCESS_USE_TLSF)
    target_compile_definitions(noza PRIVATE NOZA_PROCESS_USE_TLSF=1)
else()
    target_compile_definitions(noza PRIVATE NOZA_PROCESS_USE_TLSF=0)
endif()

if (NOZAOS_STRICT_WARNINGS)
    target_compile_options(noza PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra>
    )
endif()

if (NOZA_PLATFORM STREQUAL "rp2040")
    pico_add_extra_outputs(noza)
    pico_enable_stdio_usb(noza 0)
    pico_enable_stdio_uart(noza 1)
else()
    target_compile_options(noza PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-mcpu=cortex-m3;-mthumb;-ffunction-sections;-fdata-sections;-fno-common>
        $<$<COMPILE_LANGUAGE:CXX>:-mcpu=cortex-m3;-mthumb;-ffunction-sections;-fdata-sections;-fno-exceptions;-fno-rtti;-fno-threadsafe-statics;-fno-unwind-tables;-fno-asynchronous-unwind-tables>
        $<$<COMPILE_LANGUAGE:ASM>:-mcpu=cortex-m3;-mthumb>
    )
    target_link_options(noza PRIVATE
        -mcpu=cortex-m3
        -mthumb
        -specs=nano.specs
        -specs=nosys.specs
        -nostartfiles
        -Wl,--gc-sections
        -Wl,-Map=${CMAKE_BINARY_DIR}/noza.map
        -T${CMAKE_CURRENT_LIST_DIR}/kernel/platform/qemu_mps2_an385/linker.ld
    )
    add_custom_command(TARGET noza POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:noza> ${CMAKE_CURRENT_BINARY_DIR}/noza.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:noza> ${CMAKE_CURRENT_BINARY_DIR}/noza.bin
        COMMENT "Generating QEMU firmware images"
    )
endif()
