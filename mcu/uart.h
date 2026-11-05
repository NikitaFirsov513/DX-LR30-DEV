#ifndef MCU_UART_H
#define MCU_UART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void uart_init(uint32_t baud, void (*receive)(uint8_t), void (*error)(void));
bool uart_write(const uint8_t *data, size_t length);
uint32_t uart_error_count(void);
#endif
