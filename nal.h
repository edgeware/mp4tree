#pragma once

/*
 ******************************************************************************
 *                              NAL unit parsing                              *
 ******************************************************************************
 */

#include <stdlib.h>
#include <stdint.h>


/* Print H264 NAL unit */
void
mp4tree_sei_h264_nal_print(const uint8_t * p, size_t len, int depth);


/* Print HEVC NAL unit */
void
mp4tree_box_mdat_hevc_nal_print(const uint8_t * p, size_t len, int depth);
