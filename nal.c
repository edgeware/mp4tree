#include <stdio.h>

#include "nal.h"
#include "sei.h"
#include "common.h"
#include "mp4tree.h"

void
mp4tree_box_mdat_hevc_nal_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /*
     *  nal_unit_header( ) {        Descriptor
     *       forbidden_zero_bit          f(1)
     *       nal_unit_type               u(6)
     *       nuh_layer_id                u(6)
     *       nuh_temporal_id_plus1       u(3)
     *   }
     */

    uint8_t type = (p[0] & 0x7e) >> 1;
    uint8_t layer_id = ((p[0] & 1) << 5) | (p[1] >> 3);
    uint8_t temporal_id_plus1 = p[1] & 0x3;

    char * typestr = NULL;
    mp4tree_box_func print_func = NULL;

    switch (type)
    {
        case 0:
        case 1:
            typestr = "SLICE non-TSA non-STSA";
            break;
        case 2:
        case 3:
            typestr = "SLICE TSA";
            break;
        case 8:
            typestr = "SLICE RASL_N";
            break;
        case 9:
            typestr = "SLICE RASL_R";
            break;
        case 19:
        case 20:
            typestr = "IDR";
            break;
        case 21:
            typestr = "CRA";
            break;
        case 32:
            typestr = "VPS";
            print_func = mp4tree_hexdump;
            break;
        case 33:
            typestr = "SPS";
            print_func = mp4tree_hexdump;
            break;
        case 34:
            typestr = "PPS";
            print_func = mp4tree_hexdump;
            break;
        case 35:
            typestr = "AUD";
            print_func = mp4tree_hexdump;
            break;
        case 39:
            typestr = "PREFIX SEI";
            print_func = mp4tree_print_hevc_prefix_sei;
            break;
        case 40:
            typestr = "SUFFIX SEI";
            print_func = mp4tree_print_hevc_suffix_sei;
            break;
        default:
            typestr = "UND";
            break;
    }


    printf("%s  nal_unit_type:        %u (%s)\n", indent(depth, 0), type, typestr);
    printf("%s  nuh_layer_id:         %u\n", indent(depth, 0), layer_id);
    printf("%s  nuh_temporal_id_plus1 %u\n", indent(depth, 0), temporal_id_plus1);
    if (print_func)
    {
        print_func(p, len, depth);
    }
}

void
mp4tree_sei_h264_nal_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /*
     * nal_unit( NumBytesInNALunit ) {
     *   forbidden_zero_bit   f(1)
     *   nal_ref_idc          u(2)
     *   nal_unit_type        u(5)
     * }
     */

    const uint8_t nal_ref_idc   = (p[0] >> 5) & 0x03;
    const uint8_t nal_unit_type = p[0] & 0x1f;
    char * typestr = NULL;
    mp4tree_box_func print_func = NULL;

    switch (nal_unit_type)
    {
        case 1:
            typestr = "Non-IDR";
            break;
        case 2:
            typestr = "DPA";
            break;
        case 3:
            typestr = "DPB";
            break;
        case 4:
            typestr = "DPC";
            break;
        case 5:
            typestr = "IDR";
            break;
        case 6:
            typestr = "SEI";
            print_func = mp4tree_print_h264_sei;
            break;
        case 7:
            typestr = "SPS";
            print_func = mp4tree_hexdump;
            break;
        case 8:
            typestr = "PPS";
            print_func = mp4tree_hexdump;
            break;
        case 9:
            typestr = "AUD";
            break;
        case 10:
            typestr = "End Of Sequence";
            break;
        case 11:
            typestr = "End Of Stream";
            break;
        case 12:
            typestr = "Filler";
            break;
        case 13:
            typestr = "SPS Ext";
            break;
        case 19:
            typestr = "Aux Slice";
            break;
        default:
            typestr = "Unknown";
            break;
    }

    printf("%s  nal_ref_idc:          %u\n", indent(depth, 0), nal_ref_idc);
    printf("%s  nal_unit_type:        %u (%s)\n", indent(depth, 0), nal_unit_type, typestr);
    if (print_func)
    {
        print_func(p, len, depth);
    }
}


