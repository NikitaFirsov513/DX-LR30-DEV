#ifndef MCU_GPIO_H
#define MCU_GPIO_H
#include "stm32f1xx.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct { GPIO_TypeDef *port; uint8_t bit; } gpio_pin_t;
/* RM0008 GPIO CRL/CRH nibble: CNF[1:0], MODE[1:0]. */
typedef enum {
    GPIO_INPUT_FLOAT = 0x4,
    GPIO_INPUT_PULL = 0x8,
    GPIO_OUTPUT_PP = 0x3,
    GPIO_OUTPUT_PP_2MHZ = 0x2,
    GPIO_AF_PP = 0xb
} gpio_mode_t;

void gpio_config(gpio_pin_t pin, gpio_mode_t mode, bool initial);
void gpio_write(gpio_pin_t pin, bool high);
bool gpio_read(gpio_pin_t pin);
#endif
