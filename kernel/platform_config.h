#pragma once

#if defined(NOZA_PLATFORM_RP2040)
#define NOZA_PLATFORM_NAME "rp2040"
#define NOZA_OS_NUM_CORES 2
#define NOZA_OS_ENABLE_IRQ 1
#define NOZA_PLATFORM_IRQ_COUNT 26
#define NOZA_IRQ_UART0 20u
#elif defined(NOZA_PLATFORM_QEMU_MPS2_AN385)
#define NOZA_PLATFORM_NAME "qemu_mps2_an385"
#define NOZA_OS_NUM_CORES 1
#define NOZA_OS_ENABLE_IRQ 0
#define NOZA_PLATFORM_IRQ_COUNT 32
#define NOZA_IRQ_UART0 0u
#else
#error "Unsupported NOZA platform"
#endif

#define NOZA_MAX_IRQ_COUNT 32
