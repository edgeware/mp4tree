#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef void (*mp4tree_box_func)
(
    const uint8_t * p,
    size_t          len,
    int             depth
);

void
mp4tree_hexdump(
    const uint8_t * p,
    size_t          len,
    int             depth);
