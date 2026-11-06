#ifndef DEVICE_RADIO_H
#define DEVICE_RADIO_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { RADIO_STARTING, RADIO_RECEIVING, RADIO_TRANSMITTING, RADIO_FAULT } radio_state_t;
typedef void (*radio_receive_fn)(const uint8_t *data, uint8_t length);

/* Main-loop-only snapshot. Counters accumulate since boot, including reinitialization.
 * TX done means local transmission finished, not acknowledged delivery.
 * RSSI/SNR are the Semtech driver's integer values for the last accepted packet.
 */
typedef struct {
    uint32_t tx_started, tx_packets, tx_bytes;
    uint32_t rx_packets, rx_bytes;
    uint32_t crc_errors, header_errors, rx_no_crc;
    uint32_t tx_timeouts, rx_timeouts, tx_watchdogs, transport_errors;
    int8_t last_rssi_dbm, last_snr_db;
    bool last_rx_valid;
} radio_stats_t;

void radio_get_stats(radio_stats_t *out);
bool radio_init(radio_receive_fn receive);
void radio_poll(void);
bool radio_can_send(void);
bool radio_send(const uint8_t *data, uint8_t length);
radio_state_t radio_state(void);
const char *radio_error(void);
/* Called by EXTI only. No SPI, UART or delays in this function. */
void radio_notify_irq(void);
#endif
