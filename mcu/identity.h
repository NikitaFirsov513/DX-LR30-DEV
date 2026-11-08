#ifndef MCU_IDENTITY_H
#define MCU_IDENTITY_H
#include <stdint.h>
void identity_read_uid(uint32_t words[3]);
/* Timing sample only, not a random-number generator. Safe after board_init(). */
uint32_t identity_timing_sample(void);
#endif
