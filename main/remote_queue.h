#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef void (*remote_nec_callback_t)(uint16_t address, uint16_t command, bool repeat);

/**
 * @brief Initialize the NEC receiver and queue.
 * @param cb Callback to be called when a NEC frame or repeat is received
 */
void remote_queue_init(remote_nec_callback_t cb);
