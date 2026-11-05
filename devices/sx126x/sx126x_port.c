/* Semtech's sx126x_hal_* is a transport interface; no STM32 HAL is used. */
#include "sx126x_hal.h"
#include "board.h"
#include "config.h"
#include "clock.h"
#include "spi.h"

static bool wait_ready(void)
{
    uint32_t started = clock_millis();
    while (board_radio_busy()) {
        if ((uint32_t)(clock_millis() - started) >= BOARD_RADIO_BUSY_MS) return false;
    }
    return true;
}

sx126x_hal_status_t sx126x_hal_write(const void *context, const uint8_t *command,
    uint16_t command_length, const uint8_t *data, uint16_t data_length)
{
    (void)context;
    if (!wait_ready()) return SX126X_HAL_STATUS_ERROR;
    board_radio_select(true);
    bool ok = spi_transfer(command, NULL, command_length) && spi_transfer(data, NULL, data_length);
    board_radio_select(false);
    return ok ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command,
    uint16_t command_length, uint8_t *data, uint16_t data_length)
{
    (void)context;
    if (!wait_ready()) return SX126X_HAL_STATUS_ERROR;
    board_radio_select(true);
    bool ok = spi_transfer(command, NULL, command_length) && spi_transfer(NULL, data, data_length);
    board_radio_select(false);
    return ok ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_reset(const void *context)
{
    (void)context;
    board_radio_reset(false);
    clock_delay_ms(20);
    board_radio_reset(true);
    clock_delay_ms(20);
    return wait_ready() ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void *context)
{
    (void)context;
    const uint8_t command[] = {0xc0, 0};
    board_radio_select(true);
    clock_delay_ms(1);
    bool ok = spi_transfer(command, NULL, sizeof(command));
    board_radio_select(false);
    return ok && wait_ready() ? SX126X_HAL_STATUS_OK : SX126X_HAL_STATUS_ERROR;
}

int sx126x_get_busy(void) { return board_radio_busy(); }
