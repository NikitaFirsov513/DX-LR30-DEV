#include "radio.h"
#include "radio_config.h"
#include "sx126x_hal.h"
#include "board.h"
#include "clock.h"
#include "critical.h"
#include <stddef.h>

static radio_state_t state = RADIO_STARTING;
static radio_receive_fn receive_callback;
static volatile bool irq_pending;
static uint32_t tx_started;
static const char *last_error;
static uint8_t received[255];

static bool fail(const char *operation)
{
    last_error = operation;
    state = RADIO_FAULT;
    board_radio_off();
    return false;
}

#define REQUIRE(call) do { if ((call) != SX126X_STATUS_OK) return fail(#call); } while (0)

static bool packet_params(uint8_t size)
{
    sx126x_pkt_params_lora_t params = {
        .preamble_len_in_symb = RADIO_PREAMBLE_SYMBOLS,
        .header_type = SX126X_LORA_PKT_EXPLICIT,
        .pld_len_in_bytes = size,
        .crc_is_on = RADIO_CRC_ENABLED,
        .invert_iq_is_on = RADIO_INVERT_IQ
    };
    REQUIRE(sx126x_set_lora_pkt_params(NULL, &params));
    return true;
}

static bool enter_receive(void)
{
    board_radio_rx();
    /* Preserve factory RX timeout value 0 (single RX, restarted after each packet). */
    REQUIRE(sx126x_set_rx(NULL, 0));
    state = RADIO_RECEIVING;
    return true;
}

bool radio_init(radio_receive_fn receive)
{
    receive_callback = receive;
    state = RADIO_STARTING;
    last_error = NULL;
    irq_pending = false;
    if (sx126x_reset(NULL) != SX126X_STATUS_OK) return fail("reset/BUSY");
    if (sx126x_wakeup(NULL) != SX126X_STATUS_OK) return fail("wakeup/SPI");
    REQUIRE(sx126x_set_standby(NULL, SX126X_STANDBY_CFG_RC));
    REQUIRE(sx126x_set_standby(NULL, SX126X_STANDBY_CFG_XOSC));
    REQUIRE(sx126x_set_reg_mode(NULL, SX126X_REG_MODE_DCDC));
    REQUIRE(sx126x_set_buffer_base_address(NULL, 0, 0));
    REQUIRE(sx126x_set_pkt_type(NULL, SX126X_PKT_TYPE_LORA));
    REQUIRE(sx126x_set_trimming_capacitor_values(NULL, 0x04, 0x2f));
    sx126x_mod_params_lora_t modulation = {
        .sf = RADIO_SPREADING_FACTOR, .bw = RADIO_BANDWIDTH,
        .cr = RADIO_CODING_RATE, .ldro = 0
    };
    REQUIRE(sx126x_set_lora_mod_params(NULL, &modulation));
    if (!packet_params(255)) return false;
    sx126x_pa_cfg_params_t pa = {.pa_duty_cycle=0x04, .hp_max=0x07, .device_sel=0x00, .pa_lut=0x01};
    REQUIRE(sx126x_set_pa_cfg(NULL, &pa));
    const sx126x_irq_mask_t mask = SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE |
                                  SX126X_IRQ_TIMEOUT | SX126X_IRQ_CRC_ERROR | SX126X_IRQ_HEADER_ERROR;
    REQUIRE(sx126x_set_dio_irq_params(NULL, mask, mask, SX126X_IRQ_NONE, SX126X_IRQ_NONE));
    REQUIRE(sx126x_clear_irq_status(NULL, SX126X_IRQ_ALL));
    REQUIRE(sx126x_set_tx_params(NULL, RADIO_POWER_DBM, SX126X_RAMP_3400_US));
    REQUIRE(sx126x_set_rf_freq(NULL, RADIO_FREQUENCY_HZ));
    return enter_receive();
}

bool radio_send(const uint8_t *data, uint8_t length)
{
    if (!data || !length || state != RADIO_RECEIVING) return false;
    REQUIRE(sx126x_set_standby(NULL, SX126X_STANDBY_CFG_RC));
    REQUIRE(sx126x_clear_irq_status(NULL, SX126X_IRQ_ALL));
    if (!packet_params(length)) return false;
    REQUIRE(sx126x_write_buffer(NULL, 0, data, length));
    board_radio_tx();
    REQUIRE(sx126x_set_tx(NULL, RADIO_TX_TIMEOUT_MS));
    tx_started = clock_millis();
    state = RADIO_TRANSMITTING;
    return true;
}

void radio_notify_irq(void) { irq_pending = true; }

static bool handle_irq(void)
{
    sx126x_irq_mask_t irq;
    REQUIRE(sx126x_get_irq_status(NULL, &irq));
    REQUIRE(sx126x_clear_irq_status(NULL, SX126X_IRQ_ALL));
    if (!irq) return true;
    REQUIRE(sx126x_set_standby(NULL, SX126X_STANDBY_CFG_RC));
    if ((irq & SX126X_IRQ_RX_DONE) && !(irq & (SX126X_IRQ_CRC_ERROR | SX126X_IRQ_HEADER_ERROR))) {
        sx126x_rx_buffer_status_t buffer;
        REQUIRE(sx126x_get_rx_buffer_status(NULL, &buffer));
        REQUIRE(sx126x_read_buffer(NULL, buffer.buffer_start_pointer, received, buffer.pld_len_in_bytes));
        /* Restore RX payload limit after transmitting a shorter packet. */
        if (!packet_params(255) || !enter_receive()) return false;
        if (receive_callback) receive_callback(received, buffer.pld_len_in_bytes);
        return true;
    }
    if (!packet_params(255)) return false;
    return enter_receive();
}

void radio_poll(void)
{
    if (state == RADIO_FAULT) return;
    uint32_t saved = irq_lock();
    bool pending = irq_pending;
    irq_pending = false;
    irq_restore(saved);
    if (pending && !handle_irq()) return;
    if (state == RADIO_TRANSMITTING && (uint32_t)(clock_millis() - tx_started) >= RADIO_TX_WATCHDOG_MS) {
        /* Recover a lost DIO1 edge without inventing a successful delivery. */
        if (sx126x_set_standby(NULL, SX126X_STANDBY_CFG_RC) != SX126X_STATUS_OK) {
            fail("TX watchdog/SPI");
            return;
        }
        if (packet_params(255)) (void)enter_receive();
    }
}

bool radio_can_send(void) { return state == RADIO_RECEIVING; }
radio_state_t radio_state(void) { return state; }
const char *radio_error(void) { return last_error ? last_error : "unknown"; }
