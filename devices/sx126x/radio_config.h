#ifndef DEVICE_RADIO_CONFIG_H
#define DEVICE_RADIO_CONFIG_H
#include "sx126x.h"

/* Both boards must use this profile. CRC is required on received packets too. */
#define RADIO_FREQUENCY_HZ     868100000U
#define RADIO_SPREADING_FACTOR SX126X_LORA_SF9
#define RADIO_BANDWIDTH        SX126X_LORA_BW_125
#define RADIO_CODING_RATE      SX126X_LORA_CR_4_6
#define RADIO_PREAMBLE_SYMBOLS 8U
#define RADIO_CRC_ENABLED      true
#define RADIO_INVERT_IQ        false
#define RADIO_POWER_DBM        10
#define RADIO_TX_TIMEOUT_MS    6000U
#define RADIO_TX_WATCHDOG_MS   6500U
#endif
