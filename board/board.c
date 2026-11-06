#include "board.h"
#include "clock.h"
#include "timer.h"
#include "exti.h"

/* These peripherals use reset pin mappings. Moving them requires reviewing AFIO. */
#define DEFAULT_PIN(name, port, bit) \
    _Static_assert((unsigned)BOARD_##name##_PORT_ID == (unsigned)BOARD_PORT_##port && BOARD_##name##_BIT == bit, \
                   "Review peripheral AFIO mapping for " #name)
DEFAULT_PIN(SPI1_SCK, A, 5);
DEFAULT_PIN(SPI1_MISO, A, 6);
DEFAULT_PIN(SPI1_MOSI, A, 7);
DEFAULT_PIN(USART1_TX, A, 9);
DEFAULT_PIN(USART1_RX, A, 10);
_Static_assert(BOARD_RADIO_DIO1_BIT >= 10 && BOARD_RADIO_DIO1_BIT <= 15,
               "DIO1 must use the EXTI15_10 vector or exti.c must change");

#define BOARD_PIN(name, port, bit, mode, initial) const gpio_pin_t BOARD_##name = {GPIO##port, bit};
#include "pins.def"
#undef BOARD_PIN

void board_init(void)
{
    clock_init();
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN |
                     RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;
    (void)RCC->APB2ENR;
    /* One owner for AFIO mapping. Reset defaults, JTAG disabled, SWD retained. */
    AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
#define BOARD_PIN(name, port, bit, mode, initial) gpio_config(BOARD_##name, GPIO_##mode, initial);
#include "pins.def"
#undef BOARD_PIN
    timer_init();
}

bool board_key_pressed(void) { return !gpio_read(BOARD_KEY); }

void board_radio_rx(void)
{
    gpio_write(BOARD_RADIO_TXEN, false);
    gpio_write(BOARD_RADIO_RXEN, true);
}

void board_radio_tx(void)
{
    gpio_write(BOARD_RADIO_RXEN, false);
    gpio_write(BOARD_RADIO_TXEN, true);
}

void board_radio_off(void)
{
    gpio_write(BOARD_RADIO_RXEN, false);
    gpio_write(BOARD_RADIO_TXEN, false);
}

bool board_radio_busy(void) { return gpio_read(BOARD_RADIO_BUSY); }
void board_radio_reset(bool released) { gpio_write(BOARD_RADIO_NRST, released); }
void board_radio_select(bool selected) { gpio_write(BOARD_RADIO_NSS, !selected); }
void board_radio_irq_init(void (*notify)(void)) { exti_radio_init(BOARD_RADIO_DIO1, notify); }
