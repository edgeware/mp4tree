#pragma once

/*
 ******************************************************************************
 *                            SEI NAL unit parsing                            *
 ******************************************************************************
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


/* Print HEVC prefix SEI NAL */
void
mp4tree_print_hevc_prefix_sei(const uint8_t * p, size_t len, int depth);

/* Print HEVC suffix SEI NAL */
void
mp4tree_print_hevc_suffix_sei(const uint8_t * p, size_t len, int depth);

/* Print H264 SEI NAL */
void
mp4tree_print_h264_sei(const uint8_t * p, size_t len, int depth);

