#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#include "atom-desc.h"

/*
 ******************************************************************************
 *                             Defines                                        *
 ******************************************************************************
 */
#define array_len(_a) (sizeof(_a)/sizeof(_a[0]))


/*
 ******************************************************************************
 *                             Types                                          *
 ******************************************************************************
 */
typedef void (*mp4tree_box_func)
(
    const uint8_t * p,
    size_t          len,
    int             depth
);

typedef struct mp4tree_box_map_struct
{
    char         type[4];
    mp4tree_box_func func;
} mp4tree_box_map_t;


static struct options_struct
{
    const char * filter;
    const char * filename;
    int          truncate;
    bool         selftest;
} g_options;

static mp4tree_box_func mdat_printer = NULL;
/*
 ******************************************************************************
 *                           Function definitions                             *
 ******************************************************************************
 */

static void
mp4tree_print(const uint8_t * p, size_t len, int depth);

static mp4tree_box_func
mp4tree_box_printer_get(const uint8_t *p);

/*
 ******************************************************************************
 *                             Utility functions                              *
 ******************************************************************************
 */
static inline uint16_t
get_u16(const uint8_t * p)
{
    return (p[0] << 8) | p[1];
}

static inline uint32_t
get_u24(const uint8_t * p)
{
    return (p[0] << 16) | (p[1] << 8) | p [2];
}


static inline uint32_t
get_u32(const uint8_t * p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static inline uint64_t
get_u64(const uint8_t * p)
{
    return  ((uint64_t)p[0] << 56) |
            ((uint64_t)p[1] << 48) |
            ((uint64_t)p[2] << 40) |
            ((uint64_t)p[3] << 32) |
            ((uint64_t)p[4] << 24) |
            ((uint64_t)p[5] << 16) |
            ((uint64_t)p[6] << 8)  |
            (uint64_t) p[7];
}

static const char *
get_pascal_string(const uint8_t * p)
{
    uint16_t length = p[0];
    (void) length;
    return (char *)p + 1;
}

static uint8_t
get_bit(const uint8_t * p, int n)
{

    return (p[n/8] & (1 << (7-(n % 8)))) ? 1 : 0;
}

static inline uint32_t
get_exp_golomb(const uint8_t *p, uint32_t * bit)
{
    int      leading_zero_bits = 0;
    int      i;
    int      j;
    uint32_t v = 0;

    for (i = *bit; 1; i++)
    {

//        printf("bit%d %u\n", i, get_bit(p, i));
        if (get_bit(p, i) == 0)
            leading_zero_bits++;
        else
            break;
    }

    for (j = 0; j < leading_zero_bits; j++)
    {
//        printf("bit%d=%u ", i + j + 1, get_bit(p, i + j + 1));
        v |= get_bit(p, i + j + 1) << (leading_zero_bits -j -1);
//        printf("v=%x ", v);
    }

//    printf(" 0s=%d  v=%u ", leading_zero_bits, v);
    *bit += 2 * leading_zero_bits + 1;
//    printf("bits = %u", *bit);
    return (1 << leading_zero_bits) - 1 + v;
}

static inline const uint8_t *
get_type(const uint8_t * p)
{
    return p + 4;
}

static const char *
indent(int depth, int header)
{
    static char buf[64];
    int i = 0;

    memset(buf, 0 , sizeof buf);

    for (i = 0; i < depth; i++)
    {
        strcat(buf, "|  ");
    }

    if (header)
        strcat(buf, "+");
    else
        strcat(buf, "");

    return buf;
}

/*
 ******************************************************************************
 *                             Box printers                                   *
 ******************************************************************************
 */

static void
mp4tree_table_print(
    const char *    name,
    const char *    header,
    const uint8_t * p,
    int             esize,
    int             width,
    int             num,
    int             depth)
{
    int i;
    int j;
    int offset = 0;

    printf("%s  %s:\n", indent(depth, 0), name);
    printf("%s             %s\n", indent(depth, 0), header);
    for (i = 0; i < num; i++)
    {
        printf("%s      %3d:", indent(depth, 0), i+1);
        for (j = 0; j < width; j++)
        {
            printf("   %6u", get_u32(p + offset));
            offset += esize;
        }
        printf("\n");
    }
}

static const char *
mp4tree_hexstr(
    const uint8_t * p,
    size_t          len)
{
    static char buffer[128];
    int         n = 0;
    size_t      i;

    if (len > g_options.truncate)
        len = g_options.truncate;

    for (i = 0; i < len; i++)
    {
        n += snprintf(buffer + n, sizeof(buffer) - n, " %.2x", p[i]);
    }

    return buffer;
}


static void
mp4tree_hexdump(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    size_t i;
    size_t truncated_len = 0;

    // String templates to keep pretty alignment
    const char * empty_hex = "                                                ";
    const char * empty_txt = "                ";
    char hex_buf[1024];
    char txt_buf[1024];
    sprintf(hex_buf, "%s", empty_hex);
    sprintf(txt_buf, "%s", empty_txt);

    if (len > g_options.truncate)
    {
        truncated_len = len - g_options.truncate;
        len = g_options.truncate;
    }

    size_t line_pos = 0;
    for (i = 0; i < len; i++)
    {
        line_pos = i & 0x0f;

        // Print hex output to temporary buffer...
        char hex[4];
        sprintf(hex, "%02x ", p[i]);
        // ...and use strncpy to avoid null-termination
        strncpy(&hex_buf[line_pos*3], hex, 3);

        if (isprint(p[i]))
        {
            // A printable character
            txt_buf[line_pos] = p[i];
        }
        else
        {
            // Replace non-printable characters with a dot.
            txt_buf[line_pos] = '.';
        }

        if (i % 16 == 0)
        {
            printf("%s  %.4zx    ", indent(depth, 0), i);
        }

        if (line_pos == 15)
        {
            printf("%s |%s|\n", hex_buf, txt_buf);
            sprintf(hex_buf, "%s", empty_hex);
            sprintf(txt_buf, "%s", empty_txt);
        }
    }

    if (line_pos != 15)
    {
        printf("%s |%s|\n", hex_buf, txt_buf);
    }

    if (truncated_len)
    {
        printf("%s   ... %zu bytes truncated\n",
               indent(depth, 0), truncated_len);
    }
}

static void
mp4tree_box_print(
    const uint8_t * type,
    size_t          len,
    int             depth)
{
    const char *desc;

    printf("%s--- Length: %zu Type: %c%c%c%c\n",
            indent(depth, 1), len,
           type[0], type[1], type[2], type[3]);

    desc = get_box_desc(type);
    if (desc)
        printf("%s  Description: %s\n", indent(depth + 1, 0), desc);
}


static void
mp4tree_box_mdat_h264_nal_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /*
     * nal_unit( NumBytesInNALunit ) {
     *   forbidden_zero_bit   f(1)
     *   nal_ref_idc          u(2)
     *   nal_unit_type        u(5)
     */

    const uint8_t nal_ref_idc   = (p[0] >> 5) & 0x03;
    const uint8_t nal_unit_type = p[0] & 0x1f;
    char * typestr = NULL;
    bool hexdump = false;

    switch (nal_unit_type)
    {
        case 1:
        case 2:
        case 3:
            typestr = "Non-IDR";
            break;
        case 4:
            typestr = "IDR";
            break;
        case 6:
            typestr = "SEI";
            break;
        case 7:
            typestr = "SPS";
            hexdump= true;
            break;
        case 8:
            typestr = "PPS";
            hexdump= true;
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
        default:
            typestr = "Unknown";
            break;
    }

    printf("%s  nal_ref_idc:    %u\n", indent(depth, 0), nal_ref_idc);
    printf("%s  nal_unit_type:  %u (%s)\n", indent(depth, 0), nal_unit_type, typestr);
    if (hexdump)
    {
        mp4tree_hexdump(p, len, depth);
    }
}

static void
mp4tree_box_mdat_h264_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const uint8_t * p_end = p + len;
    while (p < p_end)
    {
        uint32_t nal_length = get_u32(p);

        printf("%s--- Length %u Type: H264 NAL\n", indent(depth, 1), nal_length);
        mp4tree_box_mdat_h264_nal_print(p+4, nal_length, depth+1);
        p += nal_length + 4;
    }
}

static void
mp4tree_box_mdat_hevc_nal_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /*
     *  nal_unit_header( ) {        Descriptor
     *      forbidden_zero_bit          f(1)
     *       nal_unit_type               u(6)
     *       nuh_layer_id                u(6)
     *       nuh_temporal_id_plus1       u(3)
     *   }
     */

    uint8_t type = (p[0] >> 1) & 0x3f;
    uint8_t layer_id = ((p[0] & 1) << 5) | (p[1] >> 3);
    uint8_t temporal_id_plus1 = p[1] & 0x3;

    char * typestr = NULL;
    bool hexdump = false;

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
        case 32:
            typestr = "VPS";
            hexdump= true;
            break;
        case 33:
            typestr = "SPS";
            hexdump = true;
            break;
        case 34:
            typestr = "PPS";
            hexdump = true;
            break;
        case 35:
            typestr = "AUD";
            hexdump = true;
            break;
        case 39:
        case 40:
            typestr = "SEI";
            break;
        default:
            typestr = "UND";
            break;
    }


    printf("%s  nal_unit_type:        %u (%s)\n", indent(depth, 0), type, typestr);
    printf("%s  nuh_layer_id:         %u\n", indent(depth, 0), layer_id);
    printf("%s  nuh_temporal_id_plus1 %u\n", indent(depth, 0), temporal_id_plus1);
    if (hexdump)
    {
        mp4tree_hexdump(p, len, depth);
    }
}

static void
mp4tree_box_mdat_hevc_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{

    const uint8_t * p_end = p + len;

    while (p < p_end)
    {
        uint32_t nal_length = get_u32(p);
        p += 4;

        printf("%s--- Length %u Type: HEVC NAL\n", indent(depth, 1), nal_length);
        mp4tree_box_mdat_hevc_nal_print(p, nal_length, depth + 1);
        p += nal_length;
    }
}

static void
mp4tree_box_senc_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const uint32_t  flags        = get_u24(p+1);
    uint32_t        sample_count = get_u32(p+4);
    uint32_t        i       = 0;
    uint32_t        j       = 0;

/*
    aligned(8) class SampleEncryptionBox
    extends FullBox(‘senc’, version=0, flags)
    {
        unsigned int(32) sample_count;
        {
            unsigned int(Per_Sample_IV_Size*8) InitializationVector;
            if (flags & 0x000002)
            {
                unsigned int(16) subsample_count;
                {
                    unsigned int(16) BytesOfClearData;
                    unsigned int(32) BytesOfProtectedData;
                } [ subsample_count ]
            }
        }[ sample_count ]
    }
*/

    printf("%s  Version:      %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:        0x%.6x\n", indent(depth, 0), flags);
    printf("%s  Sample Count: %u\n", indent(depth, 0), sample_count);

    p += 8;
//    printf("%s  Sample\n", indent(depth, 0), j);
    for (i = 0; i < sample_count; i++)
    {
        /* Print 8 bytes IV */
        printf("%s Sample: %3u\n",
               indent(depth, 1), i);
        printf("%s  IV:     %s\n",
               indent(depth+1, 0), mp4tree_hexstr(p, 8));
        p += 8;

        if (flags & 0x000002)
        {
            uint32_t sub_sample_count = get_u16(p);
            printf("%s  Subsample Count: %u\n", indent(depth+1, 1), sub_sample_count);
            p += 2;
            printf("%s  Subsample  BytesOfClear  BytesOfProtectedData\n", indent(depth+2, 0));
            for (j = 0; j < sub_sample_count; j++)
            {
                printf("%s  %9d  %12u  %20u\n",
                       indent(depth+2, 0), j, get_u16(p), get_u32(p+2));
                p +=6;
            }
        }
    }
}


static void
mp4tree_box_uuid_sample_encryption(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const uint32_t flags       = get_u24(p+1);
    uint32_t       num_entries = 0;
    uint32_t       i           = 0;
    uint8_t        iv_size     = 8;

    printf("%s  Name:        Sample Encryption Box\n", indent(depth, 0));
    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.6x\n", indent(depth, 0), flags);

    p += 4;
    if (flags & 1)
    {
        printf("%s  AlgorithmID: 0x%.2x%.2x%.2x\n",indent(depth, 0),
                 p[0], p[1], p[2]);
        printf("%s  IV Sizes:       %u\n", indent(depth, 0), p[3]);

        printf("%s  Key ID:\n", indent(depth, 0));
        mp4tree_hexdump(p+4, 16, depth);
        p += 20;
    }

    num_entries = get_u32(p);
    p  += 4;

    printf("%s  Num Entries: %u\n", indent(depth, 0), num_entries);

    printf("%s  Entry           IV             Entries\n",
            indent(depth, 0));

    for (i = 0; i < num_entries; i++)
    {
        if (iv_size)
        {
            printf("%s Entry: %3u\n",
                   indent(depth, 1), i);
            printf("%s  IV:     %s\n",
               indent(depth+1, 0), mp4tree_hexstr(p, 8));
            p += iv_size;
        }

        if (flags & 2)
        {
            uint32_t j;
            uint16_t num_sub_samples;

            num_sub_samples = get_u16(p);
            printf("%s  Sub-Entries Count: %u\n", indent(depth+1, 1), num_sub_samples);
            p += 2;

            printf("%s  Sub-Entry  BytesOfClear  BytesOfProtectedData\n", indent(depth+2, 0));
            for (j = 0; j < num_sub_samples; j++)
            {
                printf("%s  %9d  %12u  %20u\n",
                       indent(depth+2, 0), j, get_u16(p), get_u32(p+2));
                p +=6;
            }
        }
    }
}

static void
mp4tree_box_uuid_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    struct
    {
        const uint8_t    uuid[16];
        mp4tree_box_func func;
    } uuids[] =
    {
        {
            {0xa2, 0x39, 0x4f, 0x52, 0x5a, 0x9b, 0x4f, 0x14,
             0xa2, 0x44, 0x6c, 0x42, 0x7c, 0x64, 0x8d, 0xf4},
            mp4tree_box_uuid_sample_encryption
        },
        /*
        {
            {0x6d, 0x1d, 0x9b, 0x05, 0x42, 0xd5, 0x44, 0xe6,
             0x80, 0xe2, 0x14, 0x1d, 0xaf, 0xf7, 0x57, 0xb2},
            mp4tree_hexdump
        }*/
    };
    int i;

    for (i = 0; i < array_len(uuids); i++)
    {
        if ( memcmp(uuids[i].uuid, p, 16) == 0)
            return uuids[i].func(p + 16, len - 16, depth);
    }

    return mp4tree_hexdump(p, len, depth);
}

#if 0
static void
mp4tree_box_stsd_avc1_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /* Version
       A 16-bit integer indicating the version number of the compressed data.
       This is set to 0, unless a compressor has changed its data format.*/
    printf("%s  Version:          %u\n", indent(depth, 0), get_u16(p));

    /* Revision level
       A 16-bit integer that must be set to 0. */
    printf("%s  Revision level:   %u\n", indent(depth, 0), get_u16(p+2));

    /* Vendor
       A 32-bit integer that specifies the developer of the compressor that
       generated the compressed data. Often this field contains 'appl' to
       indicate Apple, Inc. */
    printf("%s  Vendor:           %x\n", indent(depth, 0), get_u32(p+4));

    /* Temporal quality
       A 32-bit integer containing a value from 0 to 1023 indicating the degree
       of temporal compression. */
    printf("%s  Temporal Quality: %u\n", indent(depth, 0), get_u32(p+8));

    /* Spatial quality
       A 32-bit integer containing a value from 0 to 1024 indicating the degree
       of spatial compression.*/
    printf("%s  Spatial Quality:  %u\n", indent(depth, 0), get_u32(p+12));

    /* Width
       A 16-bit integer that specifies the width of the source image in pixels.*/
    printf("%s  Width:            %u\n", indent(depth, 0), get_u16(p+16));

    /* Height
       A 16-bit integer that specifies the height of the source image in pixels. */
    printf("%s  Heigth:           %u\n", indent(depth, 0), get_u16(p+18));

    /* Horizontal resolution
       A 32-bit fixed-point number containing the horizontal resolution of the
       image in pixels per inch. */
    printf("%s  Horizontal PPI:   %u\n", indent(depth, 0), get_u32(p+20));

    /* Vertical resolution
       A 32-bit fixed-point number containing the vertical resolution of the
       image in pixels per inch. */
    printf("%s  Vertical PPI:     %u\n", indent(depth, 0), get_u32(p+24));

    /* Data size
       A 32-bit integer that must be set to 0. */
    printf("%s  Data Size:        %u\n", indent(depth, 0), get_u32(p+28));
    /* Frame count
       A 16-bit integer that indicates how many frames of compressed data are
       stored in each sample. Usually set to 1. */
    printf("%s  Frame Count:      %u\n", indent(depth, 0), get_u16(p+32));

    /* Compressor name
       A 32-byte Pascal string containing the name of the compressor that
       created the image, such as "jpeg". */
    printf("%s  Compressor:       %x\n", indent(depth, 0), get_u32(p+34));

    /* Depth
       A 16-bit integer that indicates the pixel depth of the compressed image.
       Values of 1, 2, 4, 8 ,16, 24, and 32 indicate the depth of color images.
       The value 32 should be used only if the image contains an alpha channel.
       Values of 34, 36, and 40 indicate 2-, 4-, and 8-bit grayscale,
       respectively, for grayscale images. */
    printf("%s  Depth:            %x\n", indent(depth, 0), get_u16(p+36));

    /* Color table ID
       A 16-bit integer that identifies which color table to use. If this
       field is set to –1, the default color table should be used for the
       specified depth. For all depths below 16 bits per pixel, this indicates
       a standard Macintosh color table for the specified depth. Depths of 16,
       24, and 32 have no color table. If the color table ID is set to 0, a
       color table is contained within the sample description itself.
       The color table immediately follows the color table ID field in the
       sample description. See Color Table Atoms for a complete description of
       a color table. */

    printf("%s  Color Table ID:   %x\n", indent(depth, 0), get_u16(p+38));
    mdat_printer = mp4tree_box_mdat_h264_print;
    mp4tree_box_print((uint8_t *)"avc1", len, depth-1);
    mp4tree_hexdump(p, len, depth);
}

#endif

static void
mp4tree_box_stsd_avcC_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    mp4tree_hexdump(p, len, depth);
    mdat_printer = mp4tree_box_mdat_h264_print;
}


static void
mp4tree_box_stsd_hvcC_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    mp4tree_hexdump(p, len, depth);
    mdat_printer = mp4tree_box_mdat_hevc_print;
}


static void
mp4tree_box_ctab_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Color Table Seed   %x\n", indent(depth, 0), get_u32(p));
    printf("%s  Color Table Flags  %u\n", indent(depth, 0), get_u16(p+4));
    printf("%s  Color Table Size   %u\n", indent(depth, 0), get_u16(p+6));
}



#if 0

static void
mp4tree_box_stsd_hev1_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    // 6 bytes reserved to
    p += 6;

    printf("%s  Data reference index:          %u\n", indent(depth, 0), get_u16(p));

    p += 2;
    /* Version
       A 16-bit integer indicating the version number of the compressed data.
       This is set to 0, unless a compressor has changed its data format.*/
    printf("%s  Version:          %u\n", indent(depth, 0), get_u16(p));

    /* Revision level
       A 16-bit integer that must be set to 0. */
    printf("%s  Revision level:   %u\n", indent(depth, 0), get_u16(p+2));

    /* Vendor
       A 32-bit integer that specifies the developer of the compressor that
       generated the compressed data. Often this field contains 'appl' to
       indicate Apple, Inc. */
    printf("%s  Vendor:           %x\n", indent(depth, 0), get_u32(p+4));

    /* Temporal quality
       A 32-bit integer containing a value from 0 to 1023 indicating the degree
       of temporal compression. */
    printf("%s  Temporal Quality: %u\n", indent(depth, 0), get_u32(p+8));

    /* Spatial quality
       A 32-bit integer containing a value from 0 to 1024 indicating the degree
       of spatial compression.*/
    printf("%s  Spatial Quality:  %u\n", indent(depth, 0), get_u32(p+12));

    /* Width
       A 16-bit integer that specifies the width of the source image in pixels.*/
    printf("%s  Width:            %u\n", indent(depth, 0), get_u16(p+16));

    /* Height
       A 16-bit integer that specifies the height of the source image in pixels. */
    printf("%s  Heigth:           %u\n", indent(depth, 0), get_u16(p+18));

    /* Horizontal resolution
       A 32-bit fixed-point number containing the horizontal resolution of the
       image in pixels per inch. */
    printf("%s  Horizontal PPI:   %u\n", indent(depth, 0), get_u32(p+20));

    /* Vertical resolution
       A 32-bit fixed-point number containing the vertical resolution of the
       image in pixels per inch. */
    printf("%s  Vertical PPI:     %u\n", indent(depth, 0), get_u32(p+24));

    /* Data size
       A 32-bit integer that must be set to 0. */
    printf("%s  Data Size:        %u\n", indent(depth, 0), get_u32(p+28));
    /* Frame count
       A 16-bit integer that indicates how many frames of compressed data are
       stored in each sample. Usually set to 1. */
    printf("%s  Frame Count:      %u\n", indent(depth, 0), get_u16(p+32));

    /* Compressor name
       A 32-byte Pascal string containing the name of the compressor that
       created the image, such as "jpeg". */
    printf("%s  Compressor:       %x\n", indent(depth, 0), get_u32(p+34));

    /* Depth
       A 16-bit integer that indicates the pixel depth of the compressed image.
       Values of 1, 2, 4, 8 ,16, 24, and 32 indicate the depth of color images.
       The value 32 should be used only if the image contains an alpha channel.
       Values of 34, 36, and 40 indicate 2-, 4-, and 8-bit grayscale,
       respectively, for grayscale images. */
    printf("%s  Depth:            %x\n", indent(depth, 0), get_u16(p+36));

    /* Color table ID
       A 16-bit integer that identifies which color table to use. If this
       field is set to –1, the default color table should be used for the
       specified depth. For all depths below 16 bits per pixel, this indicates
       a standard Macintosh color table for the specified depth. Depths of 16,
       24, and 32 have no color table. If the color table ID is set to 0, a
       color table is contained within the sample description itself.
       The color table immediately follows the color table ID field in the
       sample description. See Color Table Atoms for a complete description of
       a color table. */

    printf("%s  Color Table ID:   %x\n", indent(depth, 0), get_u16(p+38));
    mp4tree_box_ctab_print(p + 40, len, depth);
//    mp4tree_print(p + 40, len - 40, depth + 1);
//    mp4tree_box_print((uint8_t *)"hvc1", len, depth-1);
    mp4tree_hexdump(p + 48, len, depth);

    mdat_printer = mp4tree_box_mdat_hevc_print;
}
#endif

static void
mp4tree_box_saio_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /*
     * aligned(8) class SampleAuxiliaryInformationOffsetsBox
     * extends FullBox(‘saio’, version, flags)
     * {
     *     if (flags & 1) {
     *         unsigned int(32) aux_info_type;
     *         unsigned int(32) aux_info_type_parameter;
     *     }
     *     unsigned int(32) entry_count;
     *     if ( version == 0 ) {
     *         unsigned int(32) offset[ entry_count ];
     *     }
     *     else {
     *         unsigned int(64) offset[ entry_count ];
     *     }
     * }
     *
     */
    uint8_t  version = p[0];
    uint32_t flags = get_u24(p+1);
    uint8_t entry_count = 0;

    printf("%s  Version:                  %u\n",indent(depth, 0), version);
    printf("%s  Flags:                    0x%.6x\n", indent(depth, 0), flags);
    p += 4;

    if (flags & 1)
    {
        printf("%s  Aux Info Type:            %c%c%c%c\n",indent(depth, 0), p[0], p[1], p[2], p[3]);
        printf("%s  Aux Info Type Parameter:  %u\n",indent(depth, 0), get_u32(p+4));
        p += 8;
    }

    entry_count = get_u32(p);
    p += 4;
    if (version == 0)
    {
        int i = 0;

        printf("%s  Entry     Offset\n", indent(depth, 0));
        for (i = 0; i < entry_count; i++)
        {
            printf("%s  %3d:       %u\n", indent(depth, 0), i, get_u32(p));
            p += 4;
        }
    }
    else
    {
        int i = 0;

        printf("%s  Entry     Offset\n", indent(depth, 0));
        for (i = 0; i < entry_count; i++)
        {
            printf("%s  %3d:       %llu\n", indent(depth, 0), i, (long long unsigned) get_u64(p));
            p += 8;
        }
    }
}


static void
mp4tree_box_saiz_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /*
     * aligned(8) class SampleAuxiliaryInformationSizesBox
     *  extends FullBox(‘saiz’, version = 0, flags)
     *  {
     *       if (flags & 1) {
     *           unsigned int(32) aux_info_type;
     *           unsigned int(32) aux_info_type_parameter;
     *       }
     *   unsigned int(8) default_sample_info_size;
     *   unsigned int(32) sample_count;
     *   if (default_sample_info_size == 0) {
     *       unsigned int(8) sample_info_size[ sample_count ];
     *   }
     * }
     *
     **/
    uint32_t flags = get_u24(p+1);

    printf("%s  Version:                  %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:                    0x%.6x\n", indent(depth, 0), flags);
    p += 4;
    if (flags & 1)
    {
        printf("%s  Aux Info Type:            %c%c%c%c\n",indent(depth, 0), p[0], p[1], p[2], p[3]);
        printf("%s  Aux Info Type Parameter:  %u\n",indent(depth, 0), get_u32(p+4));
        p += 8;
    }
    uint8_t default_sample_info_size = p[0];
    uint8_t sample_count = get_u32(p + 1);
    printf("%s  Default Sample Info Size: %u\n",indent(depth, 0), default_sample_info_size);
    printf("%s  Sample Count:             %u\n",indent(depth, 0), sample_count);

    p += 5;
    if (default_sample_info_size == 0)
    {
        int i = 0;

        printf("%s  Sample     Sample Info Size\n", indent(depth, 0));
        for (i = 0; i < sample_count; i++)
        {
            printf("%s  %3d:           %.2u\n", indent(depth, 0), i, p[i]);
        }
    }
}


static void
mp4tree_box_btrt_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Version:                 %u\n",indent(depth, 0), p[0]);
    mp4tree_hexdump(p, len, depth);

}

static void
mp4tree_box_frma_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Data Format: %c%c%c%c\n",indent(depth, 0), p[0], p[1], p[2], p[3]);
}

static void
mp4tree_box_tenc_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    uint32_t flags = get_u24(p+1);
    uint32_t is_encrypted = get_u24(p+4);

    printf("%s  Version:       %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:         0x%.6x\n", indent(depth, 0), flags);
    printf("%s  IsEncrypted:   %u\n", indent(depth, 0), is_encrypted);
    printf("%s  IV_Size:       %u\n", indent(depth, 0), p[7]);

    printf("%s  Default Key ID %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\n",
          indent(depth, 0),
          p[8], p[9],p[10], p[11], p[12], p[13], p[14],p[15],
          p[16], p[17], p[18],p[19], p[20], p[21], p[22], p[23]);
}

static void
mp4tree_box_schm_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    uint32_t        flags       = get_u24(p+1);

    printf("%s  Version:       %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:         0x%.6x\n", indent(depth, 0), flags);

    printf("%s  Scheme Type:    %c%c%c%c\n",indent(depth, 0), p[4], p[5], p[6], p[7]);
    printf("%s  Scheme Version: %u\n", indent(depth, 0), get_u32(p+8));

    if (flags & 0x1)
    {
        printf("%s  Scheme URI: TODO\n", indent(depth, 0));
    }
}

static void
mp4tree_box_stsd_sample_audio_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /* General sample decription */
    printf("%s  Reserved:             %.2x%.2x%.2x%.2x%.2x%.2x\n",
               indent(depth, 0), p[0], p[1], p[2], p[3], p[4], p[5]);

    printf("%s  Data reference index: %u\n", indent(depth, 0), get_u16(p+6));

    p += 8;

/*
*    Version
*    A 16-bit integer that holds the sample description version (currently 0 or 1).
*
*    Revision level
*    A 16-bit integer that must be set to 0.
*
*    Vendor
*    A 32-bit integer that must be set to 0.
*
*    Number of channels
*    A 16-bit integer that indicates the number of sound channels
*    used by the sound sample. Set to 1 for monaural sounds, 2 for stereo sounds.
*    Higher numbers of channels are not supported.
*
*    Sample size (bits)
*    A 16-bit integer that specifies the number of bits in each uncompressed
*    sound sample. Allowable values are 8 or 16. Formats using more than 16
*    bits per sample set this field to 16 and use sound description version 1.
*
*    Compression ID
*    A 16-bit integer that must be set to 0 for version 0 sound descriptions.
*    This may be set to –2 for some version 1 sound descriptions; see Redefined Sample Tables.
*
*    Packet size
*    A 16-bit integer that must be set to 0.
*
*    Sample rate
*    A 32-bit unsigned fixed-point number (16.16) that indicates the rate at
*    which the sound samples were obtained. The integer portion of this number
*    should match the media’s time scale. Many older version 0 files have values
*    of 22254.5454 or 11127.2727, but most files have integer values,
*    such as 44100. Sample rates greater than 2^16 are not supported.
*
*    Version 0 of the sound description format assumes uncompressed audio in 'raw ' or 'twos' format, 1 or 2 channels, 8 or 16 bits per sample, and a compression ID of 0.
*
*
*/
    uint16_t version = get_u16(p);
    printf("%s  Version:              %u\n", indent(depth, 0), version);
    printf("%s  Revision level:       %u\n", indent(depth, 0), get_u16(p+2));
    printf("%s  Vendor:               %x\n", indent(depth, 0), get_u32(p+4));
    printf("%s  Number of Channels:   %u\n", indent(depth, 0), get_u16(p+8));
    printf("%s  Sample Size:          %u\n", indent(depth, 0), get_u16(p+10));
    printf("%s  Compression ID:       %u\n", indent(depth, 0), get_u16(p+12));
    printf("%s  Packet Size:          %u\n", indent(depth, 0), get_u16(p+14));
    printf("%s  Sample Rate:          %u\n", indent(depth, 0), get_u32(p+16));

    if (version == 0)
    {
        mp4tree_print(p+20, len - 28, depth);
    }
}


static void
mp4tree_box_stsd_sample_video_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    /* General sample decription */
    printf("%s  Reserved:             %.2x%.2x%.2x%.2x%.2x%.2x\n",
               indent(depth, 0), p[0], p[1], p[2], p[3], p[4], p[5]);

    printf("%s  Data reference index: %u\n", indent(depth, 0), get_u16(p+6));

    p += 8;

    /* Video sample description */
    /* Version
       A 16-bit integer indicating the version number of the compressed data.
       This is set to 0, unless a compressor has changed its data format.*/
    printf("%s  Version:          %u\n", indent(depth, 0), get_u16(p));

    /* Revision level
       A 16-bit integer that must be set to 0. */
    printf("%s  Revision level:   %u\n", indent(depth, 0), get_u16(p+2));

    /* Vendor
       A 32-bit integer that specifies the developer of the compressor that
       generated the compressed data. Often this field contains 'appl' to
       indicate Apple, Inc. */
    printf("%s  Vendor:           %x\n", indent(depth, 0), get_u32(p+4));

    /* Temporal quality
       A 32-bit integer containing a value from 0 to 1023 indicating the degree
       of temporal compression. */
    printf("%s  Temporal Quality: %u\n", indent(depth, 0), get_u32(p+8));

    /* Spatial quality
       A 32-bit integer containing a value from 0 to 1024 indicating the degree
       of spatial compression.*/
    printf("%s  Spatial Quality:  %u\n", indent(depth, 0), get_u32(p+12));

    /* Width
       A 16-bit integer that specifies the width of the source image in pixels.*/
    printf("%s  Width:            %u\n", indent(depth, 0), get_u16(p+16));

    /* Height
       A 16-bit integer that specifies the height of the source image in pixels. */
    printf("%s  Heigth:           %u\n", indent(depth, 0), get_u16(p+18));

    /* Horizontal resolution
       A 32-bit fixed-point number containing the horizontal resolution of the
       image in pixels per inch. */
    printf("%s  Horizontal PPI:   %u\n", indent(depth, 0), get_u32(p+20));

    /* Vertical resolution
       A 32-bit fixed-point number containing the vertical resolution of the
       image in pixels per inch. */
    printf("%s  Vertical PPI:     %u\n", indent(depth, 0), get_u32(p+24));

    /* Data size
       A 32-bit integer that must be set to 0. */
    printf("%s  Data Size:        %u\n", indent(depth, 0), get_u32(p+28));
    /* Frame count
       A 16-bit integer that indicates how many frames of compressed data are
       stored in each sample. Usually set to 1. */
    printf("%s  Frame Count:      %u\n", indent(depth, 0), get_u16(p+32));

    /* Compressor name
       A 32-byte Pascal string containing the name of the compressor that
       created the image, such as "jpeg". */
    printf("%s  Compressor:       %s\n", indent(depth, 0), get_pascal_string(p+34));

    /* Depth
       A 16-bit integer that indicates the pixel depth of the compressed image.
       Values of 1, 2, 4, 8 ,16, 24, and 32 indicate the depth of color images.
       The value 32 should be used only if the image contains an alpha channel.
       Values of 34, 36, and 40 indicate 2-, 4-, and 8-bit grayscale,
       respectively, for grayscale images. */
    printf("%s  Depth:            %x\n", indent(depth, 0), get_u16(p+68));

    /* Color table ID
       A 16-bit integer that identifies which color table to use. If this
       field is set to –1, the default color table should be used for the
       specified depth. For all depths below 16 bits per pixel, this indicates
       a standard Macintosh color table for the specified depth. Depths of 16,
       24, and 32 have no color table. If the color table ID is set to 0, a
       color table is contained within the sample description itself.
       The color table immediately follows the color table ID field in the
       sample description. See Color Table Atoms for a complete description of
       a color table. */

    printf("%s  Color Table ID:   %x\n", indent(depth, 0), get_u16(p+70));

    mp4tree_print(p+70, len - 78, depth);
//    mp4tree_hexdump(p+72, len - 78, depth);

}



static void
mp4tree_box_stsd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    uint32_t        flags       = get_u24(p+1);
    const uint32_t  num_entries = get_u32(p+4);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.6x\n", indent(depth, 0), flags);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num_entries);

    /* Print recursive boxes */
    mp4tree_print(p + 8, len - 8, depth);
}

static void
mp4tree_box_ftyp_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const uint8_t  * pp;

    printf("%s  Major brand:   %c%c%c%c\n",
               indent(depth, 0), p[0], p[1], p[2], p[3]);
    printf("%s  Minor version: %u\n",indent(depth, 0), get_u32(p + 4));

    for (pp = p + 8; pp < p + len; pp += 4)
    {
        printf("%s  Compability brand: %c%c%c%c\n",
               indent(depth, 0), pp[0], pp[1], pp[2], pp[3]);
    }
}

static void
mp4tree_box_mfhd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Sequence Number: %u\n", indent(depth, 0), get_u32(p+4));
}

static void
mp4tree_box_mvhd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    mp4tree_hexdump(p, 128, depth);

    printf("%s  Version:            %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:              0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Creation time:      %u\n",indent(depth, 0), get_u32(p+4));
    printf("%s  Modification time:  %u\n",indent(depth, 0), get_u32(p+8));
    printf("%s  Time scale:         %u\n",indent(depth, 0), get_u32(p+12));
    printf("%s  Duration:           %u\n",indent(depth, 0), get_u32(p+16));
    printf("%s  Preferred rate:     %u\n",indent(depth, 0), get_u32(p+20));
    printf("%s  Preferred volume:   %u\n",indent(depth, 0), get_u16(p+24));
//    printf("%s  Matrix structure:   %u\n",indent(depth, 0), get_u32(p+2));
    printf("%s  Preview time:       %u\n",indent(depth, 0), get_u32(p+72));
    printf("%s  Preview duration:   %u\n",indent(depth, 0), get_u32(p+76));
    printf("%s  Poster time:        %u\n",indent(depth, 0), get_u32(p+80));
    printf("%s  Selection time:     %u\n",indent(depth, 0), get_u32(p+84));
    printf("%s  Selection duration: %u\n",indent(depth, 0), get_u32(p+88));
    printf("%s  Current Time:       %u\n",indent(depth, 0), get_u32(p+92));
    printf("%s  Next track ID       %u\n",indent(depth, 0), get_u32(p+96));
}

static void
mp4tree_box_iods_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Version:            %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:              0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    mp4tree_hexdump(p, len, depth);
}

static void
mp4tree_box_mdhd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Version:            %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:              0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Creation time:      %u\n",indent(depth, 0), get_u32(p+4));
    printf("%s  Modification time:  %u\n",indent(depth, 0), get_u32(p+8));
    printf("%s  Time scale:         %u\n",indent(depth, 0), get_u32(p+12));
    printf("%s  Duration:           %u\n",indent(depth, 0), get_u32(p+16));
    printf("%s  Language:           %u\n",indent(depth, 0), get_u16(p+20));
    printf("%s  Quality:            %u\n",indent(depth, 0), get_u16(p+22));
}

static void
mp4tree_box_vmhd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Version:      %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:        0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Graphic mode: %u\n",indent(depth, 0), get_u16(p+4));
    printf("%s  Opcolor       TODO\n",indent(depth, 0));
}

static void
mp4tree_box_tfhd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    uint32_t        flags       = get_u24(p+1);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.6x\n", indent(depth, 0), flags);

    /*
     *aligned(8) class TrackFragmentHeaderBox
     *extends FullBox(tfhd, 0, tf_flags){
     *  unsigned int(32) track_ID;
     *  all the following are optional fields
     *  unsigned int(64) base_data_offset;
     *  unsigned int(32) sample_description_index;
     *  unsigned int(32) default_sample_duration;
     *  unsigned int(32) default_sample_size;
     *  unsigned int(32) default_sample_flags
     * }
     */
}


static void
mp4tree_box_tkhd_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Version:            %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:              0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Creation time:      %u\n",indent(depth, 0), get_u32(p+4));
    printf("%s  Modification time:  %u\n",indent(depth, 0), get_u32(p+8));
    printf("%s  Track ID:           %u\n",indent(depth, 0), get_u32(p+12));
    /* Reserved 4 bytes */
    printf("%s  Duration:           %u\n",indent(depth, 0), get_u32(p+20));
    /* Reserved 8 bytes */
    printf("%s  Layer:              %u\n",indent(depth, 0), get_u16(p+32));
    printf("%s  Alternate groupe:   %u\n",indent(depth, 0), get_u16(p+34));
    printf("%s  Volume:             %u\n",indent(depth, 0), get_u16(p+36));
    /* Reserved 2 bytes */

//    printf("%s  Matrix structure:   %u\n",indent(depth, 0), get_u32(p+2));
    printf("%s  Track width:        %u\n",indent(depth, 0), get_u32(p+76));
    printf("%s  Track height:       %u\n",indent(depth, 0), get_u32(p+80));
}

static void
mp4tree_box_trun_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    uint32_t        flags    = get_u24(p+1);
    const uint32_t  samples  = get_u32(p+4);
    char table_hdr[128] = {0};
    int  table_fields = 0;

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.6x\n", indent(depth, 0), flags);
    printf("%s  Samples:     %u\n", indent(depth, 0), samples);

    p +=8;

    if (flags & 1)
    {
        printf("%s  Data Offset: %u\n", indent(depth, 0), get_u32(p));
        p += 4;
    }

    if (flags & 0x100)
    {
        strcat(table_hdr, "Duration    ");
        table_fields++;
    }

    if (flags & 0x200)
    {
        strcat(table_hdr, "Size    ");
        table_fields++;
    }

    if (flags & 0x400)
    {
        strcat(table_hdr, "Flags    ");
        table_fields++;
    }

    if (flags & 0x800)
    {
        strcat(table_hdr, "Composition-Time-Offset (CTS)");
        table_fields++;
    }

    mp4tree_table_print("Sample Table", table_hdr,
                        p, 4, table_fields, samples, depth);

}


static void
mp4tree_box_hdlr_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    printf("%s  Version:                %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:                  0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Component type:         %u\n",indent(depth, 0), get_u32(p+4));
    printf("%s  Component subtype:      %u\n",indent(depth, 0), get_u32(p+8));
    printf("%s  Component manufacturer: %u\n",indent(depth, 0), get_u32(p+8));
    printf("%s  Component flags:        %u\n",indent(depth, 0), get_u32(p+12));
    printf("%s  Component flags mask:   %u\n",indent(depth, 0), get_u32(p+16));
    printf("%s  Component name:         %u\n",indent(depth, 0), get_u32(p+12));
}

static void
mp4tree_box_stts_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int num = get_u32(p+4);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);

    mp4tree_table_print("Time-to-sample table",
                        "Sample count | Sample duration",
                        p + 8, 4, 2, num, depth);
}

static void
mp4tree_box_ctts_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int num = get_u32(p+4);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);
    printf("%s  Composition-offset table:\n", indent(depth, 0));
    printf("%s        Sample count | Composition offset\n", indent(depth, 0));

    mp4tree_table_print("Composition-offset table",
                        "Sample count | Composition offset",
                        p + 8, 4, 2, num, depth);
}

static void
mp4tree_box_stsc_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int num = get_u32(p+4);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);

    mp4tree_table_print("Composition-offset table",
                        "First chunk | Samples per chunk | Sample Description ID",
                        p + 8, 4, 3, num, depth);
}

static void
mp4tree_box_stsz_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int sample_size = get_u32(p+4);
    const int num         = get_u32(p+8);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Sample size: %u\n", indent(depth, 0), sample_size);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);

    mp4tree_table_print("Sample size table",
                        "Size",
                        p + 12, 4, 1, num, depth);
}

static void
mp4tree_box_stco_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int num = get_u32(p+4);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);

    mp4tree_table_print("Sample size table",
                        "Size",
                        p + 8, 4, 1, num, depth);
}

static void
mp4tree_box_stss_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int num = get_u32(p+4);

    printf("%s  Version:     %u\n",indent(depth, 0), p[0]);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);

    mp4tree_table_print("Sync sample table",
                        "Size",
                        p + 8, 4, 1, num, depth);
}

/* 14496-12 8.7.7 */
static void
mp4tree_box_subs_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const int num = get_u32(p+4);
    const int version = p[0];
    int entry, sub;

    printf("%s  Version:     %u\n",indent(depth, 0), version);
    printf("%s  Flags:       0x%.2x%.2x%.2x\n", indent(depth, 0), p[1], p[2], p[3]);
    printf("%s  Num Entries: %u\n", indent(depth, 0), num);

    p += 8;

    for (entry = 0; entry < num; entry++) {
        const int sub_count = get_u16(p + 4);

        p += 6;
        if (sub_count)
            printf("%s      %3u      Size     Prio  Discardable\n",
                   indent(depth, 0), entry + 1);
        for (sub = 0; sub < sub_count; sub++) {
            printf("%s      %3d:", indent(depth, 0), sub + 1);
            if (version == 1) {
                printf("   %6u", get_u32(p));
                p += 4;
            } else {
                printf("   %6u", get_u16(p));
                p += 2;
            }
            printf("   %6u", p[0]);
            printf("   %6u", p[1]);
            printf("\n");
            p += 6;
        }
    }
}

static void
mp4tree_box_mdat_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    if (mdat_printer != NULL)
    {
        mdat_printer(p, len, depth);
    }
    else
    {
        /* TODO: using h264 as default for now */
        mp4tree_box_mdat_hevc_print(p, len, depth);
    }
}


static mp4tree_box_func
mp4tree_box_printer_get(const uint8_t *p)
{
    static mp4tree_box_map_t box_map[] =
    {
        /* Non-null terminated 4 byte string plus function */

        { "btrt", mp4tree_box_btrt_print },
        { "ctab", mp4tree_print },
        { "enca", mp4tree_box_stsd_sample_audio_print },
        { "encv", mp4tree_box_stsd_sample_video_print },
        { "frma", mp4tree_box_frma_print },
        { "ftyp", mp4tree_box_ftyp_print },
        { "mfhd", mp4tree_box_mfhd_print },
        { "moof", mp4tree_print },
        { "moov", mp4tree_print },
        { "mp4a", mp4tree_box_stsd_sample_audio_print },
        { "mvhd", mp4tree_box_mvhd_print },
        { "iods", mp4tree_box_iods_print },
        { "mdhd", mp4tree_box_mdhd_print },
        { "hdlr", mp4tree_box_hdlr_print },
        { "tfhd", mp4tree_box_tfhd_print },
        { "trak", mp4tree_print },
        { "tkhd", mp4tree_box_tkhd_print },
        { "trun", mp4tree_box_trun_print },
        { "ctab", mp4tree_box_ctab_print },
        { "mdia", mp4tree_print },
        { "minf", mp4tree_print },
        { "stbl", mp4tree_print },
        { "traf", mp4tree_print },
        { "schi", mp4tree_print },
        { "schm", mp4tree_box_schm_print },
        { "senc", mp4tree_box_senc_print },
        { "stsd", mp4tree_box_stsd_print },
        { "avc1", mp4tree_box_stsd_sample_video_print },
        { "avcC", mp4tree_box_stsd_avcC_print },
        { "hev1", mp4tree_box_stsd_sample_video_print },
        { "hvcC", mp4tree_box_stsd_hvcC_print },
        { "stts", mp4tree_box_stts_print },
        { "ctts", mp4tree_box_ctts_print },
        { "sinf", mp4tree_print},
        { "saio", mp4tree_box_saio_print },
        { "saiz", mp4tree_box_saiz_print },
        { "stsc", mp4tree_box_stsc_print },
        { "stsz", mp4tree_box_stsz_print },
        { "stco", mp4tree_box_stco_print },
        { "stss", mp4tree_box_stss_print },
        { "subs", mp4tree_box_subs_print },
        { "tenc", mp4tree_box_tenc_print },
        { "uuid", mp4tree_box_uuid_print },
        { "vmhd", mp4tree_box_vmhd_print },
        { "mdat", mp4tree_box_mdat_print },
    };

    int i;

    for (i = 0; i < sizeof(box_map)/sizeof(box_map[0]); i++)
    {
        if (memcmp(p, box_map[i].type, 4) == 0)
            return box_map[i].func;
    }

    return mp4tree_hexdump;
}

static bool
mp4tree_match_filter(const uint8_t * box_type)
{
    char type[5] = {0};

    if (g_options.filter == NULL)
        return true;

    strncpy(type, (char *)box_type, 4);

    if (strstr(g_options.filter, type) != NULL)
    {
        return true;
    }

    return false;
}



static void
mp4tree_print(
    const uint8_t * p,
    size_t          len,
    int             depth)
{
    const uint8_t *     end  = p + len;
    mp4tree_box_func    func = NULL;

    while (p < end)
    {
        uint64_t        box_len  = get_u32(p);
        const uint8_t * box_type = get_type(p);
        const uint8_t * box_data = p + 8;

        if (box_len == 1)
        {
            box_len = get_u64(box_data);
            box_data = p + 16; ;
        }

        if (mp4tree_match_filter(box_type))
        {
            mp4tree_box_print(box_type, box_len, depth);
            func = mp4tree_box_printer_get(box_type);
            if (func)
            {
                func(box_data, box_len - 8, depth + 1);
            }
            else
            {
                mp4tree_hexdump(box_data, 16, depth);
            }
        }

        p += box_len;
    }
}


static int
mp4tree_parse_options(
    int         argc,
    char **     argv)
{
    int optix = 0;
    int c;
    static struct option options[] =
        {
            {"truncate", required_argument, 0, 't'},
            {"filter",   required_argument, 0, 'f'},
            {"help",     0,                 0, 'h'},
            {"selftest", 0,                 0, 's'},
            {0,          0,                 0,  0}
        };

    /* Set default options */
    memset(&g_options, 0, sizeof(g_options));
    g_options.truncate = 256;

    while (1)
    {
        c = getopt_long(argc, argv, "t:f:hs",
                        options, &optix);

        if (c == -1)
            break;

        switch (c)
        {
        case 't':
            g_options.truncate = atoi(optarg);
            break;
        case 'f':
            g_options.filter = optarg;
            break;
        case 's':
            g_options.selftest = true;
            return 0;
        case 'h':
        default:
            return -1;
        }
    }

    /* File name */
    if (optind < argc)
        g_options.filename = argv[optind];
    else
        return -1;

    return 0;
}


static int mp4tree_selftest()
{
    uint32_t t1 = 0xffffffff;
    uint32_t bit = 0;

    uint8_t v[] =
    {
        0x80, // 1       = 0
        0x40, // 010     = 1
        0x60, // 011     = 2
        0x20, // 00100   = 3
        0x28, // 00101   = 4
        0x30, // 00110   = 5
        0x38, // 00111   = 6
        0x10, // 0001000 = 7
        0x12, // 0001001 = 8
    };

    int i;

    for (i = 0; i < 32; i++)
    {
        if (get_bit((uint8_t *)&t1, i) != 1)
        {
            printf("Failed 1\n");
            return -1;
        }
    }

    for (i = 0; i < sizeof(v)/sizeof(v[0]); i++)
    {
        printf("%x -> %u\n", i, get_exp_golomb(&v[i], &bit));
        printf("bit = %u\n", bit);
        bit = 0;
    }



    return 0;
}

static void
mp4tree_usage_print(const char * binary)
{
    printf("Description:\n");
    printf(" This program parses and prints the content of an mp4 file.\n");
    printf("Usage: %s [OPTION]... [FILE]\n", binary);
    printf("  Available OPTIONs:\n");
    printf("  -t, --truncate=N          Truncate boxes larger N bytes (default N=256)\n");
    printf("  -s, --selftest            Run self test\n");
    printf("\n");
}

int
main(int argc, char **argv)
{
    struct      stat sb = {0};
    uint8_t *   buf     = NULL;
    int         fd      = -1;
    size_t      len     = 0;

    if (mp4tree_parse_options(argc, argv) < 0)
    {
        mp4tree_usage_print(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (g_options.selftest)
    {
        return mp4tree_selftest();
    }


    printf("Reading file %s\n", g_options.filename);

    if (stat(g_options.filename, &sb) < 0)
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    buf = malloc(sb.st_size);

    if (buf == NULL)
    {
        printf("Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    fd = open(g_options.filename, 0);

    if (fd < 0)
    {
        perror("open");
        goto errout;
    }

    len = read(fd, buf, sb.st_size);

    printf("Read %zd bytes \n", len);
    if (len < 0)
    {
        perror("read");
        goto errout;
    }

    printf("File Content:\n");
    mp4tree_print(buf, len, 0);

    if (buf != NULL)
        free(buf);

    return 0;

errout:

    if (buf != NULL)
        free(buf);
    return EXIT_FAILURE;
}

