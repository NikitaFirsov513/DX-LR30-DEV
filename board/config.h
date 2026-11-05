#ifndef PJ26_CONFIG_H
#define PJ26_CONFIG_H

/* PJ26, STM32F103C8T6. Clock changes also require reviewing mcu/clock.c. */
#define BOARD_HSE_HZ             8000000U
#define BOARD_HSI_HZ             8000000U
#define BOARD_CORE_HZ           72000000U
#define BOARD_APB1_HZ           36000000U
#define BOARD_APB2_HZ           72000000U
#define BOARD_TIMER_HZ          72000000U
#define BOARD_UART_BAUD             9600U
#define BOARD_SPI_HZ             4500000U
#define BOARD_UART_IDLE_US          5000U
#define BOARD_SPI_TIMEOUT_MS          20U
#define BOARD_UART_TIMEOUT_MS       1000U
#define BOARD_RADIO_BUSY_MS         1000U

#endif
