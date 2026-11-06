/* Execute production radio.c and Semtech driver with a simulated SPI command interface. */
#include "radio.h"
#include "radio_config.h"
#include "sx126x_hal.h"
#include "sx126x_regs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t now;
static uint16_t irq;
static bool fail_transport, incoming_crc = true;
static uint8_t fail_read_opcode;
static unsigned writes, reads, rx_switches, tx_switches, off_switches, deliveries;
static uint8_t last_packet_size, modulation[4], frequency[4], tx_data[255], tx_size;
static uint8_t rx_data[255] = {0, 0x80, 0xff}, rx_size = 3;
static uint8_t raw_snr = 0xe0; /* -8 dB */

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
    if (command[0] == 0x8c) {
        assert(command_length == 7 && command[5] == 1); /* CRC in every RX/TX configuration. */
        last_packet_size = command[4];
    }
    if (command[0] == 0x8b) { assert(command_length == 5); memcpy(modulation, command + 1, 4); }
    if (command[0] == 0x86) { assert(command_length == 5); memcpy(frequency, command + 1, 4); }
    if (command[0] == 0x0e) { assert(data_length <= 255); memcpy(tx_data, data, data_length); tx_size = (uint8_t)data_length; }
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command,
    uint16_t command_length, uint8_t *data, uint16_t data_length)
{
    (void)context;
    reads++;
    if (fail_transport || (fail_read_opcode && command[0] == fail_read_opcode)) return SX126X_HAL_STATUS_ERROR;
    memset(data, 0, data_length);
    if (command[0] == 0x12) { data[0] = (uint8_t)(irq >> 8); data[1] = (uint8_t)irq; }
    if (command[0] == 0x13) { data[0] = rx_size; data[1] = 0; }
    if (command[0] == 0x1e) { assert(data_length == rx_size); memcpy(data, rx_data, data_length); }
    if (command[0] == 0x14) { assert(data_length == 3); data[0] = 200; data[1] = raw_snr; data[2] = 204; }
    if (command[0] == 0x1d && command_length == 4 && data_length == 1) {
        uint16_t address = ((uint16_t)command[1] << 8) | command[2];
        if (address == SX126X_REG_LR_HEADER_CRC) data[0] = incoming_crc ? SX126X_REG_LR_HEADER_CRC_MASK : 0;
        if (address == SX126X_REG_LR_HEADER_CR) data[0] = (uint8_t)(RADIO_CODING_RATE << SX126X_REG_LR_HEADER_CR_POS);
    }
    return SX126X_HAL_STATUS_OK;
}

static void receive(const uint8_t *data, uint8_t length)
{
    assert(length == rx_size && !memcmp(data, rx_data, length));
    assert(radio_state() == RADIO_RECEIVING);
    deliveries++;
}

static void signal(uint16_t flags)
{
    irq = flags;
    unsigned before = writes + reads;
    radio_notify_irq();
    assert(writes + reads == before); /* No SPI work in the ISR notification. */
    radio_poll();
}

static radio_stats_t snapshot(void)
{
    radio_stats_t stats;
    radio_get_stats(&stats);
    return stats;
}

int main(void)
{
    assert(RADIO_CRC_ENABLED);
    assert(radio_init(receive) && radio_can_send());
    radio_stats_t stats = snapshot();
    assert(!stats.last_rx_valid && !stats.tx_started && !stats.rx_packets && !stats.transport_errors);
    assert(modulation[0] == 9 && modulation[1] == SX126X_LORA_BW_125 && modulation[2] == SX126X_LORA_CR_4_6);
    uint32_t rf = ((uint32_t)frequency[0] << 24) | ((uint32_t)frequency[1] << 16) |
                  ((uint32_t)frequency[2] << 8) | frequency[3];
    uint32_t expected = (uint32_t)((((uint64_t)RADIO_FREQUENCY_HZ << 25) + 16000000U) / 32000000U);
    assert(rf == expected && last_packet_size == 255);
    const uint8_t packet[] = {0, 0x80, 0xff};
    assert(!radio_send(packet, 0) && !radio_send(NULL, 3));
    assert(radio_send(packet, 3));
    assert(radio_state() == RADIO_TRANSMITTING && !radio_can_send());
    assert(!radio_send(packet, 3));
    assert(tx_switches == 1 && tx_size == 3 && !memcmp(tx_data, packet, 3));
    assert(snapshot().tx_started == 1 && snapshot().tx_packets == 0);
    signal(SX126X_IRQ_TX_DONE);
    assert(radio_can_send() && last_packet_size == 255);
    stats = snapshot();
    assert(stats.tx_started == 1 && stats.tx_packets == 1 && stats.tx_bytes == 3);
    signal(SX126X_IRQ_TX_DONE); /* Duplicate/late event cannot invent another transmission. */
    assert(snapshot().tx_packets == 1);
    signal(SX126X_IRQ_RX_DONE);
    stats = snapshot();
    assert(deliveries == 1 && stats.rx_packets == 1 && stats.rx_bytes == 3);
    assert(stats.last_rx_valid && stats.last_rssi_dbm == -100 && stats.last_snr_db == -8);
    signal(SX126X_IRQ_RX_DONE | SX126X_IRQ_CRC_ERROR);
    signal(SX126X_IRQ_HEADER_ERROR);
    stats = snapshot();
    assert(deliveries == 1 && stats.crc_errors == 1 && stats.header_errors == 1);
    assert(stats.rx_packets == 1 && stats.last_snr_db == -8);
    incoming_crc = false;
    signal(SX126X_IRQ_RX_DONE); /* Explicit-header CRC-off packets are not trusted. */
    assert(deliveries == 1 && snapshot().rx_no_crc == 1 && radio_can_send());
    incoming_crc = true;
    raw_snr = 20;
    rx_size = 255;
    for (unsigned i = 0; i < sizeof rx_data; i++) rx_data[i] = (uint8_t)i;
    signal(SX126X_IRQ_RX_DONE);
    stats = snapshot();
    assert(deliveries == 2 && stats.rx_packets == 2 && stats.rx_bytes == 258 && stats.last_snr_db == 5);
    assert(radio_send(rx_data, 255) && tx_size == 255 && !memcmp(tx_data, rx_data, 255));
    signal(SX126X_IRQ_TX_DONE);
    assert(snapshot().tx_packets == 2 && snapshot().tx_bytes == 258);
    signal(SX126X_IRQ_TIMEOUT);
    assert(snapshot().rx_timeouts == 1);
    assert(radio_send(packet, 3));
    signal(SX126X_IRQ_TIMEOUT | SX126X_IRQ_TX_DONE); /* Timeout wins over contradictory success. */
    assert(radio_can_send() && snapshot().tx_timeouts == 1 && snapshot().tx_packets == 2);
    now = 0xfffffff0U;
    assert(radio_send(packet, 3));
    now += RADIO_TX_WATCHDOG_MS - 1;
    radio_poll();
    assert(radio_state() == RADIO_TRANSMITTING);
    now++;
    radio_poll();
    assert(radio_can_send() && snapshot().tx_watchdogs == 1 && snapshot().tx_packets == 2);
    fail_read_opcode = 0x14;
    signal(SX126X_IRQ_RX_DONE); /* A failed status read must not publish bogus quality/payload. */
    assert(radio_state() == RADIO_FAULT && deliveries == 2 && off_switches == 1);
    assert(snapshot().transport_errors == 1 && snapshot().rx_packets == 2);
    assert(snapshot().last_snr_db == 5);
    fail_read_opcode = 0;
    assert(radio_init(receive));
    fail_transport = true;
    assert(!radio_send(packet, 3) && radio_state() == RADIO_FAULT && off_switches == 2);
    assert(radio_error() && !radio_can_send());
    assert(!radio_init(receive) && radio_state() == RADIO_FAULT);
    assert(snapshot().transport_errors == 3 && snapshot().tx_started == 4);
    fail_transport = false;
    assert(radio_init(receive) && rx_switches >= 6);
    assert(snapshot().transport_errors == 3 && snapshot().rx_packets == 2); /* Retained until MCU reset. */
    puts("PASS: RF/CRC profile, strict RX CRC, binary 255-byte packets, counters/RSSI/SNR, IRQ deferral, timeout/watchdog and transport failures");
    return 0;
}
