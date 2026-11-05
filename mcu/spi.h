#ifndef MCU_SPI_H
#define MCU_SPI_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
void spi_init(void);
/* NULL tx sends zero; NULL rx discards received bytes. Always drains RXNE. */
bool spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length);
#endif
