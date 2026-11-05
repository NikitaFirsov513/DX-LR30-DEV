/* DX-SMART demo port: bounded SPI transport for PJ26, GCC. */
#include "sx126x_hal.h"
#include "driver_spi.h"
#include "driver_timer.h"
#include "UserConfig.h"
#include "main.h"

static void wait_ready(void)
{
    uint32_t started = HAL_GetTick();
    while (HAL_GPIO_ReadPin(LCC68_BUSY_PORT, LCC68_BUSY_PIN) == GPIO_PIN_SET) {
        if ((uint32_t)(HAL_GetTick() - started) > 1000U) {
            printf("RADIO ERROR: BUSY timeout on PA2; check module/pinout\r\n");
            Error_Handler();
        }
    }
}

sx126x_hal_status_t sx126x_hal_write(const void *context, const uint8_t *command,
    uint16_t command_length, const uint8_t *data, uint16_t data_length)
{
    (void)context;
    wait_ready();
    HAL_GPIO_WritePin(LCC68_NSS_PORT, LCC68_NSS_PIN, GPIO_PIN_RESET);
    if (command_length) SPI_WriteBytes((uint8_t *)command, command_length);
    if (data_length) SPI_WriteBytes((uint8_t *)data, data_length);
    HAL_GPIO_WritePin(LCC68_NSS_PORT, LCC68_NSS_PIN, GPIO_PIN_SET);
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command,
    uint16_t command_length, uint8_t *data, uint16_t data_length)
{
    (void)context;
    wait_ready();
    HAL_GPIO_WritePin(LCC68_NSS_PORT, LCC68_NSS_PIN, GPIO_PIN_RESET);
    if (command_length) SPI_WriteBytes((uint8_t *)command, command_length);
    if (data_length) SPI_ReadBytes(data, data_length);
    HAL_GPIO_WritePin(LCC68_NSS_PORT, LCC68_NSS_PIN, GPIO_PIN_SET);
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_reset(const void *context)
{
    (void)context;
    HAL_GPIO_WritePin(LCC68_NRST_PORT, LCC68_NRST_PIN, GPIO_PIN_RESET);
    ms_timer_delay(20);
    HAL_GPIO_WritePin(LCC68_NRST_PORT, LCC68_NRST_PIN, GPIO_PIN_SET);
    ms_timer_delay(20);
    wait_ready();
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup(const void *context)
{
    (void)context;
    uint8_t command[2] = {0xc0, 0};
    HAL_GPIO_WritePin(LCC68_NSS_PORT, LCC68_NSS_PIN, GPIO_PIN_RESET);
    ms_timer_delay(1);
    SPI_WriteBytes(command, sizeof(command));
    HAL_GPIO_WritePin(LCC68_NSS_PORT, LCC68_NSS_PIN, GPIO_PIN_SET);
    wait_ready();
    return SX126X_HAL_STATUS_OK;
}

int sx126x_get_busy(void)
{
    return HAL_GPIO_ReadPin(LCC68_BUSY_PORT, LCC68_BUSY_PIN);
}
