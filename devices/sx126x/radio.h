#ifndef DEVICE_RADIO_H
#define DEVICE_RADIO_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { RADIO_STARTING, RADIO_RECEIVING, RADIO_TRANSMITTING, RADIO_FAULT } radio_state_t;
typedef void (*radio_receive_fn)(const uint8_t *data, uint8_t length);

bool radio_init(radio_receive_fn receive);
void radio_poll(void);
bool radio_can_send(void);
bool radio_send(const uint8_t *data, uint8_t length);
radio_state_t radio_state(void);
const char *radio_error(void);
/* Called by EXTI only. No SPI, UART or delays in this function. */
void radio_notify_irq(void);
#endif
