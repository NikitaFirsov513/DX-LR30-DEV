#ifndef APP_SERIAL_FRAMES_H
#define APP_SERIAL_FRAMES_H
#include <stdbool.h>
#include <stdint.h>

#define FRAME_MAX_BYTES 255U
#define FRAME_QUEUE_BYTES 2000U

/* One ISR producer, main-loop consumer. Caller serializes take/discard/flush. */
typedef struct {
    uint8_t pending[FRAME_MAX_BYTES];
    uint8_t queue[FRAME_QUEUE_BYTES];
    uint16_t pending_size, head, tail, used;
    uint32_t dropped;
    uint8_t max_payload;
} serial_frames_t;

void serial_frames_init(serial_frames_t *frames);
/* Configure before enabling UART interrupts; zero is rejected. */
bool serial_frames_set_limit(serial_frames_t *frames, uint8_t limit);
void serial_frames_feed(serial_frames_t *frames, uint8_t byte);
void serial_frames_flush(serial_frames_t *frames);
void serial_frames_discard(serial_frames_t *frames);
bool serial_frames_take(serial_frames_t *frames, uint8_t *data, uint8_t *length);
#endif
