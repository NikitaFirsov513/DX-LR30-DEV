#include "serial_frames.h"
#include <string.h>

void serial_frames_init(serial_frames_t *frames) { memset(frames, 0, sizeof(*frames)); }

static void put(serial_frames_t *frames, uint8_t byte)
{
    frames->queue[frames->tail] = byte;
    frames->tail = (frames->tail + 1U) % FRAME_QUEUE_BYTES;
    frames->used++;
}

static uint8_t get(serial_frames_t *frames)
{
    uint8_t byte = frames->queue[frames->head];
    frames->head = (frames->head + 1U) % FRAME_QUEUE_BYTES;
    frames->used--;
    return byte;
}

void serial_frames_flush(serial_frames_t *frames)
{
    if (!frames->pending_size) return;
    if (frames->used + frames->pending_size + 1U <= FRAME_QUEUE_BYTES) {
        put(frames, (uint8_t)frames->pending_size);
        for (uint16_t i = 0; i < frames->pending_size; i++) put(frames, frames->pending[i]);
    } else {
        frames->dropped++; /* Drop the new whole packet; preserve queued packets. */
    }
    frames->pending_size = 0;
}

void serial_frames_feed(serial_frames_t *frames, uint8_t byte)
{
    frames->pending[frames->pending_size++] = byte;
    if (frames->pending_size == FRAME_MAX_BYTES) serial_frames_flush(frames);
}

void serial_frames_discard(serial_frames_t *frames)
{
    frames->pending_size = 0;
    frames->dropped++;
}

bool serial_frames_take(serial_frames_t *frames, uint8_t *data, uint8_t *length)
{
    if (!frames->used) return false;
    uint8_t size = get(frames);
    for (uint16_t i = 0; i < size; i++) data[i] = get(frames);
    *length = size;
    return true;
}
