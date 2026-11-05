#ifndef MCU_EXTI_H
#define MCU_EXTI_H
#include "gpio.h"
void exti_radio_init(gpio_pin_t pin, void (*notify)(void));
#endif
