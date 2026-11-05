#ifndef TEST_CRITICAL_H
#define TEST_CRITICAL_H
#include <stdint.h>
static inline uint32_t irq_lock(void) { return 0; }
static inline void irq_restore(uint32_t saved) { (void)saved; }
#endif
