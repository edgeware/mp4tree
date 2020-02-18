#include <stdio.h>

#include "sei.h"
#include "common.h"
#include "mp4tree.h"


/*
 ******************************************************************************
 *                            Internals                                       *
 ******************************************************************************
 */

typedef struct
{
    int    payload_type;
    char * description;
} sei_info;


/* HEVC prefix SEIs */
static const sei_info hevc_prefix_seis[] =
{
    {0, "Buffering period"},
    {1, "Picture timing"},
    {2, "Pan-scan rectangle"},
    {3, "Filler payload"},
    {4, "User data registered by Recommendation ITU-T T.35"},
    {5, "User data unregistered"},
    {6, "Recovery point"},
    {9, "Scene information"},
    {15, "Picture snapshot"},
    {16, "Progressive refinement segment start"},
    {17, "Progressive refinement segment end"},
    {19, "Film grain characteristics"},
    {22, "Post-filter hint"},
    {23, "Tone mapping information"},
    {45, "Frame packing arrangement"},
    {47, "Display orientation"},
    {128, "Structure of pictures information"},
    {129, "Active parameter sets"},
    {130, "Decoding unit information"},
    {131, "Temporal sub-layer zero index"},
    {133, "Scalable nesting"},
    {134, "Region refresh"},
    {135, "No display"},
    {136, "Time code"},
    {137, "Mastering display colour volume"},
    {138, "Segmented rectangular frame packing arrangement"},
    {139, "Temporal motion-constrained tile sets"},
    {140, "Chroma resampling filter hint"},
    {141, "Knee function information"},
    {142, "Colour remapping information"},
    {143, "Deinterlace field identification"},
    {160, "Layers not present"},
    {161, "Inter-layer constrained tile sets"},
    {162, "BSP nesting"},
    {163, "BSP initial arrival time"},
    {164, "Sub bitstream property"},
    {165, "Alpha channel information"},
    {166, "Overlay info"},
    {167, "Temporal MV prediction constraints"},
    {168, "Frame field info"},
    {176, "3D reference displays info"},
    {177, "Depth representation info"},
    {178, "Multiview scene info"},
    {179, "Multiview acquisition info"},
    {180, "multiview view position"},
    {181, "Alternative drpth info"},
    {-1, NULL }
};


/* HEVC suffix SEIs */
static const sei_info hevc_suffix_seis[] =
{
    {3, "Filler payload"},
    {4, "User data registered by Recommendation ITU-T T.35"},
    {5, "User data unregistered"},
    {17, "Progressive refinement segment end"},
    {22, "Post-filter hint"},
    {132, "Decoded picture hash"},
    {-1, NULL }
};


/* H.264 SEIs */
static const sei_info h264_seis[] =
{
    {0, "Buffering period"},
    {1, "Picture timing"},
    {2, "Pan-scan rectangle"},
    {3, "Filler payload"},
    {4, "User data registered by Recommendation ITU-T T.35"},
    {5, "User data unregistered"},
    {6, "Recovery point"},
    {7, "Decoded reference picture marking repetition"},
    {8, "Spare picture"},
    {9, "Scene information"},
    {10, "Sub-sequence information"},
    {11, "Sub-sequence layer characteristics"},
    {12, "Sub-sequence characteristics"},
    {13, "Full-frame freeze"},
    {14, "Full-frame freeze release"},
    {15, "Full-frame snapshot"},
    {16, "Progressive refinement segment start"},
    {17, "Progressive refinement segment end"},
    {18, "Motion-constrained slice group set"},
    {19, "Film grain characteristics"},
    {20, "Deblocking filter display preference"},
    {21, "Stereo video information"},
    {22, "Post-filter hint"},
    {23, "Tone mapping information"},
    {24, "Scalability information"},
    {25, "Sub-picture scalable layer"},
    {26, "Non-required layer representation"},
    {27, "Priority later information"},
    {28, "Layers not present"},
    {29, "Layer dependency change"},
    {30, "Scalable nesting"},
    {31, "Base-layer temporal HRD"},
    {32, "Quality layer integrity check"},
    {33, "Redundant picture property"},
    {34, "Temporal level zero dependency representation index"},
    {35, "Temporal level switching point"},
    {36, "Parallel decoding information"},
    {37, "MVC scalable nesting"},
    {38, "View scalability information"},
    {39, "Multi-view scene information"},
    {40, "Multi-view acquisition information"},
    {41, "Non-required view component"},
    {42, "View dependency change"},
    {43, "Operational points not present"},
    {44, "Base view temporal HRD"},
    {45, "Frame packing arrangement"},
    {46, "Multi-view view position"},
    {47, "Display orientation"},
    {48, "MVCD scalable nesting"},
    {49, "MVCD view scalability information"},
    {50, "Depth-representation information"},
    {51, "3D reference displays information"},
    {52, "Depth timing"},
    {53, "Depth sampling information"},
    {54, "Constrained depth parameter set identifier"},
    {-1, NULL }
};


/* Get the SEI payload type from a NAL unit buffer */
uint32_t
mp4tree_sei_payload_type(const uint8_t * p, bool is_hevc)
{
    p++;
    if (is_hevc)
        p++;

    uint32_t payload_type = 0;
    for ( ; *p == 0xff; p++)
        payload_type += 255;

    payload_type += *p;

    return payload_type;
}


/* Get the SEI type description for payload_type from sei_info_list */
const char *
mp4tree_sei_description(uint32_t payload_type,
                        const sei_info * sei_info_list)
{
    char * desc = NULL;
    const sei_info * info = sei_info_list;
    while (info->description != NULL)
    {
        if (info->payload_type == payload_type)
        {
            desc = info->description;
            break;
        }
        info++;
    }
    return desc;
}


/* Print SEI NAL info using provided SEI info list */
void
mp4tree_sei_print(const uint8_t * p, size_t len, int depth,
                  const sei_info * sei_infos, bool is_hevc)
{
    uint32_t payload_type = mp4tree_sei_payload_type(p, is_hevc);
    const char * desc = mp4tree_sei_description(payload_type, sei_infos);

    printf("%s  Payload type:         %u\n",
           indent(depth, 0), payload_type);

    printf("%s  Payload description:  %s\n",
           indent(depth, 0), desc ? desc : "Reserved");
}


/*
 ******************************************************************************
 *                            Public interface                                *
 ******************************************************************************
 */

void
mp4tree_print_hevc_prefix_sei(const uint8_t * p, size_t len, int depth)
{
    mp4tree_sei_print(p, len, depth, hevc_prefix_seis, true);
}


void
mp4tree_print_hevc_suffix_sei( const uint8_t * p, size_t len, int depth)
{
    mp4tree_sei_print(p, len, depth, hevc_suffix_seis, true);
}


void
mp4tree_print_h264_sei(const uint8_t * p, size_t len, int depth)
{
    mp4tree_sei_print(p, len, depth, h264_seis, false);
    mp4tree_hexdump(p, len, depth);
}

