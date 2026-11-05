/* Execute production radio.c and Semtech driver with a simulated SPI command interface. */
#include "radio.h"
#include "radio_config.h"
#include "sx126x_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t now;
static uint16_t irq;
static bool fail_transport;
static unsigned writes, rx_switches, tx_switches, off_switches, deliveries;
static uint8_t last_packet_size, modulation[4], frequency[4], tx_data[255], tx_size;

uint32_t clock_millis(void) { return now; }
void board_radio_rx(void) { rx_switches++; }
void board_radio_tx(void) { tx_switches++; }
void board_radio_off(void) { off_switches++; }

sx126x_hal_status_t sx126x_hal_reset(const void *context)
{ (void)context; return fail_transport ? SX126X_HAL_STATUS_ERROR : SX126X_HAL_STATUS_OK; }
sx126x_hal_status_t sx126x_hal_wakeup(const void *context)
{ (void)context; return SX126X_HAL_STATUS_OK; }

sx126x_hal_status_t sx126x_hal_write(const void *context, const uint8_t *command,
    uint16_t command_length, const uint8_t *data, uint16_t data_length)
{
    (void)context;
    assert(command_length);
    writes++;
    if (fail_transport) return SX126X_HAL_STATUS_ERROR;
    if (command[0] == 0x02) irq = 0;
    if (command[0] == 0x8c) { assert(command_length == 7); last_packet_size = command[4]; }
    if (command[0] == 0x8b) { assert(command_length == 5); memcpy(modulation, command + 1, 4); }
    if (command[0] == 0x86) { assert(command_length == 5); memcpy(frequency, command + 1, 4); }
    if (command[0] == 0x0e) { assert(data_length <= 255); memcpy(tx_data, data, data_length); tx_size = (uint8_t)data_length; }
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command,
    uint16_t command_length, uint8_t *data, uint16_t data_length)
{
    (void)context; (void)command_length;
    if (fail_transport) return SX126X_HAL_STATUS_ERROR;
    memset(data, 0, data_length);
    if (command[0] == 0x12) { data[0] = (uint8_t)(irq >> 8); data[1] = (uint8_t)irq; }
    if (command[0] == 0x13) { data[0] = 3; data[1] = 0; }
    if (command[0] == 0x1e) { assert(data_length == 3); data[0] = 0; data[1] = 0x80; data[2] = 0xff; }
    return SX126X_HAL_STATUS_OK;
}

static void receive(const uint8_t *data, uint8_t length)
{
    assert(length == 3 && data[0] == 0 && data[1] == 0x80 && data[2] == 0xff);
    assert(radio_state() == RADIO_RECEIVING);
    deliveries++;
}

static void signal(uint16_t flags)
{
    irq = flags;
    unsigned before = writes;
    radio_notify_irq();
    assert(writes == before); /* No SPI work in the ISR notification. */
    radio_poll();
}

int main(void)
{
    assert(radio_init(receive) && radio_can_send());
    assert(modulation[0] == 9 && modulation[1] == SX126X_LORA_BW_125 && modulation[2] == SX126X_LORA_CR_4_6);
    uint32_t rf = ((uint32_t)frequency[0] << 24) | ((uint32_t)frequency[1] << 16) |
                  ((uint32_t)frequency[2] << 8) | frequency[3];
    uint32_t expected = (uint32_t)((((uint64_t)RADIO_FREQUENCY_HZ << 25) + 16000000U) / 32000000U);
    assert(rf == expected && last_packet_size == 255);
    const uint8_t packet[] = {0, 0x80, 0xff};
    assert(!radio_send(packet, 0));
    assert(radio_send(packet, 3));
    assert(radio_state() == RADIO_TRANSMITTING && !radio_can_send());
    assert(!radio_send(packet, 3));
    assert(tx_switches == 1 && tx_size == 3 && !memcmp(tx_data, packet, 3));
    signal(SX126X_IRQ_TX_DONE);
    assert(radio_can_send() && last_packet_size == 255);
    signal(SX126X_IRQ_RX_DONE);
    assert(deliveries == 1 && radio_can_send());
    signal(SX126X_IRQ_RX_DONE | SX126X_IRQ_CRC_ERROR);
    assert(deliveries == 1 && radio_can_send());
    assert(radio_send(packet, 3));
    signal(SX126X_IRQ_TIMEOUT);
    assert(radio_can_send());
    now = 0xfffffff0U;
    assert(radio_send(packet, 3));
    now += RADIO_TX_WATCHDOG_MS;
    radio_poll();
    assert(radio_can_send()); /* Wrap-safe recovery of a missing TX-done edge. */
    fail_transport = true;
    assert(!radio_send(packet, 3) && radio_state() == RADIO_FAULT && off_switches == 1);
    assert(radio_error() && !radio_can_send());
    assert(!radio_init(receive) && radio_state() == RADIO_FAULT);
    fail_transport = false;
    assert(radio_init(receive) && rx_switches >= 6);
    puts("PASS: RF settings, TX/RX, binary payload, IRQ deferral, CRC rejection, timeout/watchdog and transport failures");
    return 0;
}
