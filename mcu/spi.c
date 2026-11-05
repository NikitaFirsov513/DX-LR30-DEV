#include "spi.h"
#include "clock.h"
#include "config.h"
#include "stm32f1xx.h"

_Static_assert(BOARD_APB2_HZ / 16U == BOARD_SPI_HZ, "SPI1 BR divider must match config");

void spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;
    RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST;
    /* Master, mode 0, 8 bit, MSB first, software NSS, APB2 /16. */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0 | SPI_CR1_BR_1;
    SPI1->CR2 = 0;
    SPI1->CR1 |= SPI_CR1_SPE;
}

static bool wait_flag(uint32_t flag, bool set, uint32_t started)
{
    while (((SPI1->SR & flag) != 0U) != set) {
        if ((uint32_t)(clock_millis() - started) >= BOARD_SPI_TIMEOUT_MS) return false;
    }
    return true;
}

bool spi_transfer(const uint8_t *tx, uint8_t *rx, size_t length)
{
    uint32_t started = clock_millis();
    for (size_t i = 0; i < length; i++) {
        if (!wait_flag(SPI_SR_TXE, true, started)) return false;
        *(volatile uint8_t *)&SPI1->DR = tx ? tx[i] : 0U;
        if (!wait_flag(SPI_SR_RXNE, true, started)) return false;
        uint8_t value = *(volatile uint8_t *)&SPI1->DR;
        if (rx) rx[i] = value;
    }
    return wait_flag(SPI_SR_BSY, false, started);
}
