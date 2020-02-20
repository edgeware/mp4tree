#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


/* Pointer to a buffer parsing function */
typedef void (*mp4tree_parse_func) (const uint8_t * p, size_t len, int depth);

/* Print data buffer to stdout hexdump style */
void
mp4tree_hexdump(const uint8_t * p, size_t len, int depth);

/* Print MP4 box */
void
mp4tree_print(const uint8_t * p, size_t len, int depth);

/* Run self-test */
int mp4tree_selftest();
