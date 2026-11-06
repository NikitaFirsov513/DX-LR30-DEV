#ifndef PJ26_BOARD_H
#define PJ26_BOARD_H
#include "gpio.h"
#include <stdbool.h>

enum { BOARD_PORT_A=0, BOARD_PORT_B=1, BOARD_PORT_C=2 };
#define BOARD_PIN(name, port, bit, mode, initial) \
    extern const gpio_pin_t BOARD_##name; \
    enum { BOARD_##name##_BIT=bit, BOARD_##name##_PORT_ID=BOARD_PORT_##port };
#include "pins.def"
#undef BOARD_PIN

void board_init(void);
bool board_key_pressed(void);
void board_radio_rx(void);
void board_radio_tx(void);
void board_radio_off(void);
bool board_radio_busy(void);
void board_radio_reset(bool released);
void board_radio_select(bool selected);
void board_radio_irq_init(void (*notify)(void));

#endif
