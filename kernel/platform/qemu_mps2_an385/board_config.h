#pragma once

#define QEMU_MPS2_SYSCLK_HZ        25000000u
#define QEMU_MPS2_FLASH_BASE       0x00000000u
#define QEMU_MPS2_FLASH_SIZE       0x00400000u
#define QEMU_MPS2_RAM_BASE         0x20000000u
#define QEMU_MPS2_RAM_SIZE         0x00400000u

#define QEMU_MPS2_UART0_BASE       0x40004000u
#define QEMU_MPS2_DUALTIMER0_BASE  0x40002000u

#define QEMU_MPS2_SCS_BASE         0xE000E000u
#define QEMU_MPS2_SYST_CSR         (QEMU_MPS2_SCS_BASE + 0x010u)
#define QEMU_MPS2_SYST_RVR         (QEMU_MPS2_SCS_BASE + 0x014u)
#define QEMU_MPS2_SYST_CVR         (QEMU_MPS2_SCS_BASE + 0x018u)
#define QEMU_MPS2_SCB_ICSR         (QEMU_MPS2_SCS_BASE + 0xD04u)
#define QEMU_MPS2_SCB_SHPR2        (QEMU_MPS2_SCS_BASE + 0xD1Cu)
#define QEMU_MPS2_SCB_SHPR3        (QEMU_MPS2_SCS_BASE + 0xD20u)

#define QEMU_MPS2_ICSR_PENDSVSET   (1u << 28)
#define QEMU_MPS2_ICSR_PENDSTCLR   (1u << 25)

#define CMSDK_UART_STATE_TXBF      (1u << 0)
#define CMSDK_UART_STATE_RXBF      (1u << 1)
#define CMSDK_UART_CTRL_TX_EN      (1u << 0)
#define CMSDK_UART_CTRL_RX_EN      (1u << 1)

#define SP804_CTRL_ONESHOT         (1u << 0)
#define SP804_CTRL_32BIT           (1u << 1)
#define SP804_CTRL_PRESCALE_DIV1   (0u << 2)
#define SP804_CTRL_INT_ENABLE      (1u << 5)
#define SP804_CTRL_PERIODIC        (1u << 6)
#define SP804_CTRL_ENABLE          (1u << 7)
