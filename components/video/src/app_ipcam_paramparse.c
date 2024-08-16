#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "minIni.h"
#include "app_ipcam_paramparse.h"
#include "app_ipcam_comm.h"

/**************************************************************************
 *                              M A C R O S                               *
 **************************************************************************/
#define PARAM_STRING_LEN (32)
#define PARAM_STRING_NAME_LEN (64)

// There is no "butt" or "max" in enum, so fixed value is set to the length of array.
#define VPSS_CROP_COORDINATE_MAX 2
#define VENC_BIND_MAX 3
#define AIO_I2STYPE_BUTT 3

/**************************************************************************
 *                 V A R I A B L E    D E F I N I T I O N                 *
 **************************************************************************/
static char *input_file;
static const char ParamCfgFile[] = PARAM_CFG_INI;

const char *pixel_format[PIXEL_FORMAT_MAX] = {
    [PIXEL_FORMAT_RGB_888] = "PIXEL_FORMAT_RGB_888",
    [PIXEL_FORMAT_BGR_888] = "PIXEL_FORMAT_BGR_888",
    [PIXEL_FORMAT_RGB_888_PLANAR] = "PIXEL_FORMAT_RGB_888_PLANAR",
    [PIXEL_FORMAT_BGR_888_PLANAR] = "PIXEL_FORMAT_BGR_888_PLANAR",
    [PIXEL_FORMAT_ARGB_1555] = "PIXEL_FORMAT_ARGB_1555",
    [PIXEL_FORMAT_ARGB_4444] = "PIXEL_FORMAT_ARGB_4444",
    [PIXEL_FORMAT_ARGB_8888] = "PIXEL_FORMAT_ARGB_8888",
    [PIXEL_FORMAT_RGB_BAYER_8BPP] = "PIXEL_FORMAT_RGB_BAYER_8BPP",
    [PIXEL_FORMAT_RGB_BAYER_10BPP] = "PIXEL_FORMAT_RGB_BAYER_10BPP",
    [PIXEL_FORMAT_RGB_BAYER_12BPP] = "PIXEL_FORMAT_RGB_BAYER_12BPP",
    [PIXEL_FORMAT_RGB_BAYER_14BPP] = "PIXEL_FORMAT_RGB_BAYER_14BPP",
    [PIXEL_FORMAT_RGB_BAYER_16BPP] = "PIXEL_FORMAT_RGB_BAYER_16BPP",
    [PIXEL_FORMAT_YUV_PLANAR_422] = "PIXEL_FORMAT_YUV_PLANAR_422",
    [PIXEL_FORMAT_YUV_PLANAR_420] = "PIXEL_FORMAT_YUV_PLANAR_420",
    [PIXEL_FORMAT_YUV_PLANAR_444] = "PIXEL_FORMAT_YUV_PLANAR_444",
    [PIXEL_FORMAT_YUV_400] = "PIXEL_FORMAT_YUV_400",
    [PIXEL_FORMAT_HSV_888] = "PIXEL_FORMAT_HSV_888",
    [PIXEL_FORMAT_HSV_888_PLANAR] = "PIXEL_FORMAT_HSV_888_PLANAR",
    [PIXEL_FORMAT_NV12] = "PIXEL_FORMAT_NV12",
    [PIXEL_FORMAT_NV21] = "PIXEL_FORMAT_NV21",
    [PIXEL_FORMAT_NV16] = "PIXEL_FORMAT_NV16",
    [PIXEL_FORMAT_NV61] = "PIXEL_FORMAT_NV61",
    [PIXEL_FORMAT_YUYV] = "PIXEL_FORMAT_YUYV",
    [PIXEL_FORMAT_UYVY] = "PIXEL_FORMAT_UYVY",
    [PIXEL_FORMAT_YVYU] = "PIXEL_FORMAT_YVYU",
    [PIXEL_FORMAT_VYUY] = "PIXEL_FORMAT_VYUY",
    [PIXEL_FORMAT_FP32_C1] = "PIXEL_FORMAT_FP32_C1",
    [PIXEL_FORMAT_FP32_C3_PLANAR] = "PIXEL_FORMAT_FP32_C3_PLANAR",
    [PIXEL_FORMAT_INT32_C1] = "PIXEL_FORMAT_INT32_C1",
    [PIXEL_FORMAT_INT32_C3_PLANAR] = "PIXEL_FORMAT_INT32_C3_PLANAR",
    [PIXEL_FORMAT_UINT32_C1] = "PIXEL_FORMAT_UINT32_C1",
    [PIXEL_FORMAT_UINT32_C3_PLANAR] = "PIXEL_FORMAT_UINT32_C3_PLANAR",
    [PIXEL_FORMAT_BF16_C1] = "PIXEL_FORMAT_BF16_C1",
    [PIXEL_FORMAT_BF16_C3_PLANAR] = "PIXEL_FORMAT_BF16_C3_PLANAR",
    [PIXEL_FORMAT_INT16_C1] = "PIXEL_FORMAT_INT16_C1",
    [PIXEL_FORMAT_INT16_C3_PLANAR] = "PIXEL_FORMAT_INT16_C3_PLANAR",
    [PIXEL_FORMAT_UINT16_C1] = "PIXEL_FORMAT_UINT16_C1",
    [PIXEL_FORMAT_UINT16_C3_PLANAR] = "PIXEL_FORMAT_UINT16_C3_PLANAR",
    [PIXEL_FORMAT_INT8_C1] = "PIXEL_FORMAT_INT8_C1",
    [PIXEL_FORMAT_INT8_C3_PLANAR] = "PIXEL_FORMAT_INT8_C3_PLANAR",
    [PIXEL_FORMAT_UINT8_C1] = "PIXEL_FORMAT_UINT8_C1",
    [PIXEL_FORMAT_UINT8_C3_PLANAR] = "PIXEL_FORMAT_UINT8_C3_PLANAR",
    [PIXEL_FORMAT_8BIT_MODE] = "PIXEL_FORMAT_8BIT_MODE"
};

const char *data_bitwidth[DATA_BITWIDTH_MAX] = {
    [DATA_BITWIDTH_8] = "DATA_BITWIDTH_8",
    [DATA_BITWIDTH_10] = "DATA_BITWIDTH_10",
    [DATA_BITWIDTH_12] = "DATA_BITWIDTH_12",
    [DATA_BITWIDTH_14] = "DATA_BITWIDTH_14",
    [DATA_BITWIDTH_16] = "DATA_BITWIDTH_16"
};

const char *compress_mode[COMPRESS_MODE_BUTT] = {
    [COMPRESS_MODE_NONE] = "COMPRESS_MODE_NONE",
    [COMPRESS_MODE_TILE] = "COMPRESS_MODE_TILE",
    [COMPRESS_MODE_LINE] = "COMPRESS_MODE_LINE",
    [COMPRESS_MODE_FRAME] = "COMPRESS_MODE_FRAME"
};

const char *vi_vpss_mode[VI_VPSS_MODE_BUTT] = {
    [VI_OFFLINE_VPSS_OFFLINE] = "VI_OFFLINE_VPSS_OFFLINE",
    [VI_OFFLINE_VPSS_ONLINE] = "VI_OFFLINE_VPSS_ONLINE",
    [VI_ONLINE_VPSS_OFFLINE] = "VI_ONLINE_VPSS_OFFLINE",
    [VI_ONLINE_VPSS_ONLINE] = "VI_ONLINE_VPSS_ONLINE",
    [VI_BE_OFL_POST_OL_VPSS_OFL] = "VI_BE_OFL_POST_OL_VPSS_OFL",
    [VI_BE_OFL_POST_OFL_VPSS_OFL] = "VI_BE_OFL_POST_OFL_VPSS_OFL",
    [VI_BE_OL_POST_OFL_VPSS_OFL] = "VI_BE_OL_POST_OFL_VPSS_OFL",
    [VI_BE_OL_POST_OL_VPSS_OFL] = "VI_BE_OL_POST_OL_VPSS_OFL"
};

const char *vpss_mode[VPSS_MODE_BUTT] = {
    [VPSS_MODE_SINGLE] = "VPSS_MODE_SINGLE",
    [VPSS_MODE_DUAL] = "VPSS_MODE_DUAL",
    [VPSS_MODE_RGNEX] = "VPSS_MODE_RGNEX"
};

const char *vpss_input[VPSS_INPUT_BUTT] = {
    [VPSS_INPUT_MEM] = "VPSS_INPUT_MEM",
    [VPSS_INPUT_ISP] = "VPSS_INPUT_ISP"
};

const char *sensor_type[SENSOR_BUTT] = {
    [SENSOR_NONE] = "SENSOR_NONE",
    [SENSOR_GCORE_GC1054] = "SENSOR_GCORE_GC1054",
    [SENSOR_GCORE_GC2053] = "SENSOR_GCORE_GC2053",
    [SENSOR_GCORE_GC2053_1L] = "SENSOR_GCORE_GC2053_1L",
    [SENSOR_GCORE_GC2053_SLAVE] = "SENSOR_GCORE_GC2053_SLAVE",
    [SENSOR_GCORE_GC2093] = "SENSOR_GCORE_GC2093",
    [SENSOR_GCORE_GC2093_SLAVE] = "SENSOR_GCORE_GC2093_SLAVE",
    [SENSOR_GCORE_GC4653] = "SENSOR_GCORE_GC4653",
    [SENSOR_GCORE_GC4023] = "SENSOR_GCORE_GC4023",
    [SENSOR_GCORE_GC4653_SLAVE] = "SENSOR_GCORE_GC4653_SLAVE",
    [SENSOR_NEXTCHIP_N5] = "SENSOR_NEXTCHIP_N5",
    [SENSOR_NEXTCHIP_N6] = "SENSOR_NEXTCHIP_N6",
    [SENSOR_OV_OS08A20] = "SENSOR_OV_OS08A20",
    [SENSOR_OV_OS08A20_SLAVE] = "SENSOR_OV_OS08A20_SLAVE",
    [SENSOR_OV_OV5647] = "SENSOR_OV_OV5647",
    [SENSOR_PICO_384] = "SENSOR_PICO_384",
    [SENSOR_PICO_640] = "SENSOR_PICO_640",
    [SENSOR_PIXELPLUS_PR2020] = "SENSOR_PIXELPLUS_PR2020",
    [SENSOR_PIXELPLUS_PR2100] = "SENSOR_PIXELPLUS_PR2100",
    [SENSOR_SMS_SC1346_1L] = "SENSOR_SMS_SC1346_1L",
    [SENSOR_SMS_SC1346_1L_60] = "SENSOR_SMS_SC1346_1L_60",
    [SENSOR_SMS_SC200AI] = "SENSOR_SMS_SC200AI",
    [SENSOR_SMS_SC2331_1L] = "SENSOR_SMS_SC2331_1L",
    [SENSOR_SMS_SC2335] = "SENSOR_SMS_SC2335",
    [SENSOR_SMS_SC2336] = "SENSOR_SMS_SC2336",
    [SENSOR_SMS_SC2336P] = "SENSOR_SMS_SC2336P",
    [SENSOR_SMS_SC3335] = "SENSOR_SMS_SC3335",
    [SENSOR_SMS_SC3335_SLAVE] = "SENSOR_SMS_SC3335_SLAVE",
    [SENSOR_SMS_SC3336] = "SENSOR_SMS_SC3336",
    [SENSOR_SMS_SC401AI] = "SENSOR_SMS_SC401AI",
    [SENSOR_SMS_SC4210] = "SENSOR_SMS_SC4210",
    [SENSOR_SMS_SC8238] = "SENSOR_SMS_SC8238",
    [SENSOR_SMS_SC531AI_2L] = "SENSOR_SMS_SC531AI_2L",
    [SENSOR_SMS_SC5336_2L] = "SENSOR_SMS_SC5336_2L",
    [SENSOR_SMS_SC4336P] = "SENSOR_SMS_SC4336P",
    [SENSOR_SOI_F23] = "SENSOR_SOI_F23",
    [SENSOR_SOI_F35] = "SENSOR_SOI_F35",
    [SENSOR_SOI_F35_SLAVE] = "SENSOR_SOI_F35_SLAVE",
    [SENSOR_SOI_H65] = "SENSOR_SOI_H65",
    [SENSOR_SOI_K06] = "SENSOR_SOI_K06",
    [SENSOR_SOI_Q03P] = "SENSOR_SOI_Q03P",
    [SENSOR_SONY_IMX290_2L] = "SENSOR_SONY_IMX290_2L",
    [SENSOR_SONY_IMX307] = "SENSOR_SONY_IMX307",
    [SENSOR_SONY_IMX307_2L] = "SENSOR_SONY_IMX307_2L",
    [SENSOR_SONY_IMX307_SLAVE] = "SENSOR_SONY_IMX307_SLAVE",
    [SENSOR_SONY_IMX307_SUBLVDS] = "SENSOR_SONY_IMX307_SUBLVDS",
    [SENSOR_SONY_IMX327] = "SENSOR_SONY_IMX327",
    [SENSOR_SONY_IMX327_2L] = "SENSOR_SONY_IMX327_2L",
    [SENSOR_SONY_IMX327_SLAVE] = "SENSOR_SONY_IMX327_SLAVE",
    [SENSOR_SONY_IMX327_SUBLVDS] = "SENSOR_SONY_IMX327_SUBLVDS",
    [SENSOR_SONY_IMX334] = "SENSOR_SONY_IMX334",
    [SENSOR_SONY_IMX335] = "SENSOR_SONY_IMX335",
    [SENSOR_SONY_IMX347] = "SENSOR_SONY_IMX347",
    [SENSOR_SONY_IMX385] = "SENSOR_SONY_IMX385",
    [SENSOR_VIVO_MCS369] = "SENSOR_VIVO_MCS369",
    [SENSOR_VIVO_MCS369Q] = "SENSOR_VIVO_MCS369Q",
    [SENSOR_VIVO_MM308M2] = "SENSOR_VIVO_MM308M2",
    [SENSOR_IMGDS_MIS2008] = "SENSOR_IMGDS_MIS2008",
    [SENSOR_IMGDS_MIS2008_1L] = "SENSOR_IMGDS_MIS2008_1L"
};

const char *wdr_mode[WDR_MODE_MAX] = {
    [WDR_MODE_NONE] = "WDR_MODE_NONE",
    [WDR_MODE_BUILT_IN] = "WDR_MODE_BUILT_IN",
    [WDR_MODE_QUDRA] = "WDR_MODE_QUDRA",
    [WDR_MODE_2To1_LINE] = "WDR_MODE_2To1_LINE",
    [WDR_MODE_2To1_FRAME] = "WDR_MODE_2To1_FRAME",
    [WDR_MODE_2To1_FRAME_FULL_RATE] = "WDR_MODE_2To1_FRAME_FULL_RATE",
    [WDR_MODE_3To1_LINE] = "WDR_MODE_3To1_LINE",
    [WDR_MODE_3To1_FRAME] = "WDR_MODE_3To1_FRAME",
    [WDR_MODE_3To1_FRAME_FULL_RATE] = "WDR_MODE_3To1_FRAME_FULL_RATE",
    [WDR_MODE_4To1_LINE] = "WDR_MODE_4To1_LINE",
    [WDR_MODE_4To1_FRAME] = "WDR_MODE_4To1_FRAME",
    [WDR_MODE_4To1_FRAME_FULL_RATE] = "WDR_MODE_4To1_FRAME_FULL_RATE"
};

const char *dynamic_range[DYNAMIC_RANGE_MAX] = {
    [DYNAMIC_RANGE_SDR8] = "DYNAMIC_RANGE_SDR8",
    [DYNAMIC_RANGE_SDR10] = "DYNAMIC_RANGE_SDR10",
    [DYNAMIC_RANGE_HDR10] = "DYNAMIC_RANGE_HDR10",
    [DYNAMIC_RANGE_HLG] = "DYNAMIC_RANGE_HLG",
    [DYNAMIC_RANGE_SLF] = "DYNAMIC_RANGE_SLF",
    [DYNAMIC_RANGE_XDR] = "DYNAMIC_RANGE_XDR"
};

const char *video_format[VIDEO_FORMAT_MAX] = {
    [VIDEO_FORMAT_LINEAR] = "VIDEO_FORMAT_LINEAR"
};

const char *mode_id[CVI_ID_BUTT] = {
    [CVI_ID_BASE] = "CVI_ID_BASE",
    [CVI_ID_VB] = "CVI_ID_VB",
    [CVI_ID_SYS] = "CVI_ID_SYS",
    [CVI_ID_RGN] = "CVI_ID_RGN",
    [CVI_ID_CHNL] = "CVI_ID_CHNL",
    [CVI_ID_VDEC] = "CVI_ID_VDEC",
    [CVI_ID_VPSS] = "CVI_ID_VPSS",
    [CVI_ID_VENC] = "CVI_ID_VENC",
    [CVI_ID_H264E] = "CVI_ID_H264E",
    [CVI_ID_JPEGE] = "CVI_ID_JPEGE",
    [CVI_ID_MPEG4E] = "CVI_ID_MPEG4E",
    [CVI_ID_H265E] = "CVI_ID_H265E",
    [CVI_ID_JPEGD] = "CVI_ID_JPEGD",
    [CVI_ID_VO] = "CVI_ID_VO",
    [CVI_ID_VI] = "CVI_ID_VI",
    [CVI_ID_DIS] = "CVI_ID_DIS",
    [CVI_ID_RC] = "CVI_ID_RC",
    [CVI_ID_AIO] = "CVI_ID_AIO",
    [CVI_ID_AI] = "CVI_ID_AI",
    [CVI_ID_AO] = "CVI_ID_AO",
    [CVI_ID_AENC] = "CVI_ID_AENC",
    [CVI_ID_ADEC] = "CVI_ID_ADEC",
    [CVI_ID_AUD] = "CVI_ID_AUD",
    [CVI_ID_VPU] = "CVI_ID_VPU",
    [CVI_ID_ISP] = "CVI_ID_ISP",
    [CVI_ID_IVE] = "CVI_ID_IVE",
    [CVI_ID_USER] = "CVI_ID_USER",
    [CVI_ID_PROC] = "CVI_ID_PROC",
    [CVI_ID_LOG] = "CVI_ID_LOG",
    [CVI_ID_H264D] = "CVI_ID_H264D",
    [CVI_ID_GDC] = "CVI_ID_GDC",
    [CVI_ID_PHOTO] = "CVI_ID_PHOTO",
    [CVI_ID_FB] = "CVI_ID_FB"
};

const char *vpss_crop_coordinate[VPSS_CROP_COORDINATE_MAX] = {
    [VPSS_CROP_RATIO_COOR] = "VPSS_CROP_RATIO_COOR",
    [VPSS_CROP_ABS_COOR] = "VPSS_CROP_ABS_COOR"
};

const char *aspect_ratio[ASPECT_RATIO_MAX] = {
    [ASPECT_RATIO_NONE] = "ASPECT_RATIO_NONE",
    [ASPECT_RATIO_AUTO] = "ASPECT_RATIO_AUTO",
    [ASPECT_RATIO_MANUAL] = "ASPECT_RATIO_MANUAL"
};

const char *payload_type[PT_BUTT] = {
    [PT_PCMU] = "PT_PCMU",
    [PT_1016] = "PT_1016",
    [PT_G721] = "PT_G721",
    [PT_GSM] = "PT_GSM",
    [PT_G723] = "PT_G723",
    [PT_DVI4_8K] = "PT_DVI4_8K",
    [PT_DVI4_16K] = "PT_DVI4_16K",
    [PT_LPC] = "PT_LPC",
    [PT_PCMA] = "PT_PCMA",
    [PT_G722] = "PT_G722",
    [PT_S16BE_STEREO] = "PT_S16BE_STEREO",
    [PT_S16BE_MONO] = "PT_S16BE_MONO",
    [PT_QCELP] = "PT_QCELP",
    [PT_CN] = "PT_CN",
    [PT_MPEGAUDIO] = "PT_MPEGAUDIO",
    [PT_G728] = "PT_G728",
    [PT_DVI4_3] = "PT_DVI4_3",
    [PT_DVI4_4] = "PT_DVI4_4",
    [PT_G729] = "PT_G729",
    [PT_G711A] = "PT_G711A",
    [PT_G711U] = "PT_G711U",
    [PT_G726] = "PT_G726",
    [PT_G729A] = "PT_G729A",
    [PT_LPCM] = "PT_LPCM",
    [PT_CelB] = "PT_CelB",
    [PT_JPEG] = "PT_JPEG",
    [PT_CUSM] = "PT_CUSM",
    [PT_NV] = "PT_NV",
    [PT_PICW] = "PT_PICW",
    [PT_CPV] = "PT_CPV",
    [PT_H261] = "PT_H261",
    [PT_MPEGVIDEO] = "PT_MPEGVIDEO",
    [PT_MPEG2TS] = "PT_MPEG2TS",
    [PT_H263] = "PT_H263",
    [PT_SPEG] = "PT_SPEG",
    [PT_MPEG2VIDEO] = "PT_MPEG2VIDEO",
    [PT_AAC] = "PT_AAC",
    [PT_WMA9STD] = "PT_WMA9STD",
    [PT_HEAAC] = "PT_HEAAC",
    [PT_PCM_VOICE] = "PT_PCM_VOICE",
    [PT_PCM_AUDIO] = "PT_PCM_AUDIO",
    [PT_MP3] = "PT_MP3",
    [PT_ADPCMA] = "PT_ADPCMA",
    [PT_AEC] = "PT_AEC",
    [PT_X_LD] = "PT_X_LD",
    [PT_H264] = "PT_H264",
    [PT_D_GSM_HR] = "PT_D_GSM_HR",
    [PT_D_GSM_EFR] = "PT_D_GSM_EFR",
    [PT_D_L8] = "PT_D_L8",
    [PT_D_RED] = "PT_D_RED",
    [PT_D_VDVI] = "PT_D_VDVI",
    [PT_D_BT656] = "PT_D_BT656",
    [PT_D_H263_1998] = "PT_D_H263_1998",
    [PT_D_MP1S] = "PT_D_MP1S",
    [PT_D_MP2P] = "PT_D_MP2P",
    [PT_D_BMPEG] = "PT_D_BMPEG",
    [PT_MP4VIDEO] = "PT_MP4VIDEO",
    [PT_MP4AUDIO] = "PT_MP4AUDIO",
    [PT_VC1] = "PT_VC1",
    [PT_JVC_ASF] = "PT_JVC_ASF",
    [PT_D_AVI] = "PT_D_AVI",
    [PT_DIVX3] = "PT_DIVX3",
    [PT_AVS] = "PT_AVS",
    [PT_REAL8] = "PT_REAL8",
    [PT_REAL9] = "PT_REAL9",
    [PT_VP6] = "PT_VP6",
    [PT_VP6F] = "PT_VP6F",
    [PT_VP6A] = "PT_VP6A",
    [PT_SORENSON] = "PT_SORENSON64",
    [PT_H265] = "PT_H265",
    [PT_VP8] = "PT_VP8",
    [PT_MVC] = "PT_MVC",
    [PT_PNG] = "PT_PNG",
    [PT_AMR] = "PT_AMR",
    [PT_MJPEG] = "PT_MJPEG"
};

const char *venc_bind_mode[VENC_BIND_MAX] = {
    [VENC_BIND_DISABLE] = "VENC_BIND_DISABLE",
    [VENC_BIND_VI] = "VENC_BIND_VI",
    [VENC_BIND_VPSS] = "VENC_BIND_VPSS"
};

const char *venc_rc_mode[VENC_RC_MODE_BUTT] = {
    [VENC_RC_MODE_H264CBR] = "VENC_RC_MODE_H264CBR",
    [VENC_RC_MODE_H264VBR] = "VENC_RC_MODE_H264VBR",
    [VENC_RC_MODE_H264AVBR] = "VENC_RC_MODE_H264AVBR",
    [VENC_RC_MODE_H264QVBR] = "VENC_RC_MODE_H264QVBR",
    [VENC_RC_MODE_H264FIXQP] = "VENC_RC_MODE_H264FIXQP",
    [VENC_RC_MODE_H264QPMAP] = "VENC_RC_MODE_H264QPMAP",
    [VENC_RC_MODE_H264UBR] = "VENC_RC_MODE_H264UBR",
    [VENC_RC_MODE_MJPEGCBR] = "VENC_RC_MODE_MJPEGCBR",
    [VENC_RC_MODE_MJPEGVBR] = "VENC_RC_MODE_MJPEGVBR",
    [VENC_RC_MODE_MJPEGFIXQP] = "VENC_RC_MODE_MJPEGFIXQP",
    [VENC_RC_MODE_H265CBR] = "VENC_RC_MODE_H265CBR",
    [VENC_RC_MODE_H265VBR] = "VENC_RC_MODE_H265VBR",
    [VENC_RC_MODE_H265AVBR] = "VENC_RC_MODE_H265AVBR",
    [VENC_RC_MODE_H265QVBR] = "VENC_RC_MODE_H265QVBR",
    [VENC_RC_MODE_H265FIXQP] = "VENC_RC_MODE_H265FIXQP",
    [VENC_RC_MODE_H265QPMAP] = "VENC_RC_MODE_H265QPMAP",
    [VENC_RC_MODE_H265UBR] = "VENC_RC_MODE_H265UBR"
};

const char *venc_gop_mode[VENC_GOPMODE_BUTT] = {
    [VENC_GOPMODE_NORMALP] = "VENC_GOPMODE_NORMALP",
    [VENC_GOPMODE_DUALP] = "VENC_GOPMODE_DUALP",
    [VENC_GOPMODE_SMARTP] = "VENC_GOPMODE_SMARTP",
    [VENC_GOPMODE_ADVSMARTP] = "VENC_GOPMODE_ADVSMARTP",
    [VENC_GOPMODE_BIPREDB] = "VENC_GOPMODE_BIPREDB",
    [VENC_GOPMODE_LOWDELAYB] = "VENC_GOPMODE_LOWDELAYB"
};

/**************************************************************************
 *               F U N C T I O N    D E C L A R A T I O N S               *
 **************************************************************************/
static int Convert_StrName_to_EnumNum(const char *str_name, const char *str_enum[], const int enum_upper_bound, int * const enum_num) {
    int i = 0;

    for (i = 0; i < enum_upper_bound; ++i) {
        if (str_enum[i] != NULL && strcmp(str_name, str_enum[i]) == 0) {
            *enum_num = i;

            break;
        }
    }

    if (i == enum_upper_bound) {
        APP_PROF_LOG_PRINT(LEVEL_WARN, "There is no enum type matching [%s]!\n", str_name);

        return CVI_FAILURE;
    }

    return CVI_SUCCESS;
}

static int _Load_Param_Sys(const char *file, APP_PARAM_SYS_CFG_S *Sys)
{
    CVI_U32 i = 0;
    CVI_U32 vbpoolnum = 0;
    CVI_S32 enum_num = 0;
    CVI_S32 ret = 0;
    char tmp_section[16] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading systerm config ------------------> start \n");

    Sys->bSBMEnable = ini_getl("slice_buff", "slice_buff_mode", 0, file);
    if (Sys->bSBMEnable == 1) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "SBM Enable !\n");
    }

    Sys->vb_pool_num = ini_getl("vb_config", "vb_pool_cnt", 0, file);

    for (i = 0; i < Sys->vb_pool_num; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        sprintf(tmp_section, "vb_pool_%d", i);

        Sys->vb_pool[i].bEnable = ini_getl(tmp_section, "bEnable", 1, file);
        if (!Sys->vb_pool[i].bEnable)
            continue;

        Sys->vb_pool[vbpoolnum].width = ini_getl(tmp_section, "frame_width", 0, file);
        Sys->vb_pool[vbpoolnum].height = ini_getl(tmp_section, "frame_height", 0, file);

        ini_gets(tmp_section, "frame_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, pixel_format, PIXEL_FORMAT_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][frame_fmt] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][frame_fmt] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Sys->vb_pool[vbpoolnum].fmt = enum_num;
        }

        ini_gets(tmp_section, "data_bitwidth", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, data_bitwidth, DATA_BITWIDTH_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][data_bitwidth] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][data_bitwidth] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Sys->vb_pool[vbpoolnum].enBitWidth = enum_num;
        }

        ini_gets(tmp_section, "compress_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, compress_mode, COMPRESS_MODE_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][compress_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][compress_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Sys->vb_pool[vbpoolnum].enCmpMode = enum_num;
        }

        Sys->vb_pool[vbpoolnum].vb_blk_num = ini_getl(tmp_section, "blk_cnt", 0, file);

        APP_PROF_LOG_PRINT(LEVEL_INFO, "vb_pool[%d] w=%4d h=%4d count=%d fmt=%d\n", vbpoolnum, Sys->vb_pool[vbpoolnum].width, 
            Sys->vb_pool[vbpoolnum].height, Sys->vb_pool[vbpoolnum].vb_blk_num, Sys->vb_pool[vbpoolnum].fmt);
        
        vbpoolnum++;
    }

    Sys->vb_pool_num = vbpoolnum;

    long work_sns_cnt = 0;
    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "vi_config");
    work_sns_cnt = ini_getl(tmp_section, "sensor_cnt", 0, file);

    for(i = 0; i < work_sns_cnt; i++){
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vi_vpss_mode_%d", i);
        ini_gets(tmp_section, "enMode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, vi_vpss_mode, VI_VPSS_MODE_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][enMode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][enMode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Sys->stVIVPSSMode.aenMode[i] = enum_num;
        }
    }

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "vpss_mode");

    ini_gets(tmp_section, "enMode", " ", str_name, PARAM_STRING_NAME_LEN, file);
    ret = Convert_StrName_to_EnumNum(str_name, vpss_mode, VPSS_MODE_BUTT, &enum_num);
    if (ret != CVI_SUCCESS) {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][enMode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][enMode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
        Sys->stVPSSMode.enMode = enum_num;
    }

    CVI_U32 dev_cnt = ini_getl("vpss_dev", "dev_cnt", 0, file);
    for (i = 0; i < dev_cnt; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vpss_dev%d", i);

        ini_gets(tmp_section, "aenInput", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, vpss_input, VPSS_INPUT_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][enMode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][enMode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Sys->stVPSSMode.aenInput[i] = enum_num;
        }

        Sys->stVPSSMode.ViPipe[i] = ini_getl(tmp_section, "ViPipe", 0, file);
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading systerm config ------------------>done \n\n");

    return CVI_SUCCESS;
}

static int _Load_Param_Vi(const char *file, APP_PARAM_VI_CTX_S *pViIniCfg)
{
    int i = 0;
    int enum_num = 0;
    int ret = 0;
    long int work_sns_cnt = 0;
    char tmp[16] = {0};
    char tmp_section[16] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading vi config ------------------> start \n");

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "vi_config");
    work_sns_cnt = ini_getl(tmp_section, "sensor_cnt", 0, file);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "work_sns_cnt = %ld\n", work_sns_cnt);   
    if(work_sns_cnt <= (long int)(sizeof(pViIniCfg->astSensorCfg) / sizeof(pViIniCfg->astSensorCfg[0]))) {
        pViIniCfg->u32WorkSnsCnt = work_sns_cnt;
    } else {
        APP_PROF_LOG_PRINT(LEVEL_INFO, "work_sns_cnt error (%ld)\n", work_sns_cnt);
        return CVI_FAILURE;
    }

    // APP_PROF_LOG_PRINT(LEVEL_INFO, "sensor cfg path = %s\n", DEF_INI_PATH);
    for (i = 0; i< (int)pViIniCfg->u32WorkSnsCnt; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "sensor_config%d", i);

        pViIniCfg->astSensorCfg[i].s32SnsId = i;
        /* framerate here is right ?? */
        pViIniCfg->astSensorCfg[i].s32Framerate = ini_getl(tmp_section, "framerate", 0, file);
        /* used sensor name instead of enum ?? */
        ini_gets(tmp_section, "sns_type", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, sensor_type, SENSOR_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][sns_type] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][sns_type] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astSensorCfg[i].enSnsType = enum_num;
        }

        pViIniCfg->astSensorCfg[i].MipiDev = ini_getl(tmp_section, "mipi_dev", 0, file);
        pViIniCfg->astSensorCfg[i].s32BusId = ini_getl(tmp_section, "bus_id", 0, file);
        pViIniCfg->astSensorCfg[i].s32I2cAddr = ini_getl(tmp_section, "sns_i2c_addr", -1, file);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "sensor_ID=%d enSnsType=%d MipiDev=%d s32BusId=%d s32I2cAddr=0x%x\n", i, pViIniCfg->astSensorCfg[i].enSnsType, 
            pViIniCfg->astSensorCfg[i].MipiDev, pViIniCfg->astSensorCfg[i].s32BusId, pViIniCfg->astSensorCfg[i].s32I2cAddr);
        for (int j = 0; j < 5; j++) {
            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "laneid%d", j);
            pViIniCfg->astSensorCfg[i].as16LaneId[j] = ini_getl(tmp_section, tmp, 0, file);

            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "swap%d", j);
            pViIniCfg->astSensorCfg[i].as8PNSwap[j] = ini_getl(tmp_section, tmp, 0, file);
        }
        pViIniCfg->astSensorCfg[i].bMclkEn = ini_getl(tmp_section, "mclk_en", 1, file);
        pViIniCfg->astSensorCfg[i].u8Mclk = ini_getl(tmp_section, "mclk", 0, file);
        pViIniCfg->astSensorCfg[i].u8Orien = ini_getl(tmp_section, "orien", 0, file);
        pViIniCfg->astSensorCfg[i].bHwSync = ini_getl(tmp_section, "hw_sync", 0, file);
        pViIniCfg->astSensorCfg[i].u8UseDualSns = ini_getl(tmp_section, "use_dual_Sns", 0, file);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "sensor_ID=%d bMclkEn=%d u8Mclk=%d u8Orien=%d bHwSync=%d\n", i, pViIniCfg->astSensorCfg[i].bMclkEn, 
            pViIniCfg->astSensorCfg[i].u8Mclk, pViIniCfg->astSensorCfg[i].u8Orien, pViIniCfg->astSensorCfg[i].bHwSync);
    }

    for (i = 0; i< (int)pViIniCfg->u32WorkSnsCnt; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vi_cfg_dev%d", i);
        pViIniCfg->astDevInfo[i].ViDev = ini_getl(tmp_section, "videv", 0, file);

        ini_gets(tmp_section, "wdrmode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, wdr_mode, WDR_MODE_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][wdrmode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][wdrmode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astDevInfo[i].enWDRMode = enum_num;
        }

        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vi_cfg_pipe%d", i);
        for (int j = 0; j< WDR_MAX_PIPE_NUM; j++) {
            memset(tmp, 0, sizeof(tmp));
            sprintf(tmp, "apipe%d", j);
            pViIniCfg->astPipeInfo[i].aPipe[j] = ini_getl(tmp_section, tmp, 0, file);
        }

        ini_gets(tmp_section, "pipe_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, vi_vpss_mode, VI_VPSS_MODE_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pipe_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pipe_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astPipeInfo[i].enMastPipeMode = enum_num;
        }

        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vi_cfg_chn%d", i);
        pViIniCfg->astChnInfo[i].s32ChnId = i;
        pViIniCfg->astChnInfo[i].u32Width = ini_getl(tmp_section, "width", 0, file);
        pViIniCfg->astChnInfo[i].u32Height = ini_getl(tmp_section, "height", 0, file);
        pViIniCfg->astChnInfo[i].f32Fps = ini_getl(tmp_section, "fps", 0, file);

        ini_gets(tmp_section, "pixFormat", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, pixel_format, PIXEL_FORMAT_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixFormat] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixFormat] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astChnInfo[i].enPixFormat = enum_num;
        }

        ini_gets(tmp_section, "dynamic_range", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, dynamic_range, DYNAMIC_RANGE_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dynamic_range] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dynamic_range] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astChnInfo[i].enDynamicRange = enum_num;
        }

        ini_gets(tmp_section, "video_format", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, video_format, VIDEO_FORMAT_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][video_format] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][video_format] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astChnInfo[i].enVideoFormat = enum_num;
        }

        ini_gets(tmp_section, "compress_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, compress_mode, COMPRESS_MODE_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][compress_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][compress_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pViIniCfg->astChnInfo[i].enCompressMode = enum_num;
        }

        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vi_cfg_isp%d", i);
        pViIniCfg->astIspCfg[i].bAfFliter = ini_getl(tmp_section, "af_filter", 0, file);
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading vi config ------------------> done \n\n");

    return CVI_SUCCESS;
}

static int _Load_Param_Vpss(const char *file, APP_PARAM_VPSS_CFG_T *Vpss)
{
    CVI_U32 grp_idx = 0;
    CVI_U32 chn_idx = 0;
    CVI_S32 enum_num = 0;
    CVI_S32 ret = 0;
    char tmp_section[16] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading vpss config ------------------> start \n");

    memset(tmp_section, 0, sizeof(tmp_section));
    snprintf(tmp_section, sizeof(tmp_section), "vpss_config");

    Vpss->u32GrpCnt = ini_getl(tmp_section, "vpss_grp", 0, file);

    for (grp_idx = 0; grp_idx < Vpss->u32GrpCnt; grp_idx++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vpssgrp%d", grp_idx);
        Vpss->astVpssGrpCfg[grp_idx].bEnable = ini_getl(tmp_section, "grp_enable", 1, file);
        if (!Vpss->astVpssGrpCfg[grp_idx].bEnable)
            continue;
        Vpss->astVpssGrpCfg[grp_idx].VpssGrp = grp_idx;
        Vpss->astVpssGrpCfg[grp_idx].bBindMode = ini_getl(tmp_section, "bind_mode", 0, file);
        if (Vpss->astVpssGrpCfg[grp_idx].bBindMode) {
            ini_gets(tmp_section, "src_mod_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, mode_id, CVI_ID_BUTT, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][src_mod_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][src_mod_id] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                Vpss->astVpssGrpCfg[grp_idx].astChn[0].enModId = enum_num;
            }

            Vpss->astVpssGrpCfg[grp_idx].astChn[0].s32DevId = ini_getl(tmp_section, "src_dev_id", 0, file);
            Vpss->astVpssGrpCfg[grp_idx].astChn[0].s32ChnId = ini_getl(tmp_section, "src_chn_id", 0, file);

            ini_gets(tmp_section, "dst_mod_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, mode_id, CVI_ID_BUTT, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dst_mod_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dst_mod_id] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                Vpss->astVpssGrpCfg[grp_idx].astChn[1].enModId = enum_num;
            }

            Vpss->astVpssGrpCfg[grp_idx].astChn[1].s32DevId = ini_getl(tmp_section, "dst_dev_id", 0, file);
            Vpss->astVpssGrpCfg[grp_idx].astChn[1].s32ChnId = ini_getl(tmp_section, "dst_chn_id", 0, file);
        }
        APP_PROF_LOG_PRINT(LEVEL_INFO, "vpss grp_idx=%d\n", grp_idx);
        VPSS_GRP_ATTR_S *pstVpssGrpAttr = &Vpss->astVpssGrpCfg[grp_idx].stVpssGrpAttr;

        ini_gets(tmp_section, "pixel_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, pixel_format, PIXEL_FORMAT_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixel_fmt] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][pixel_fmt] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            pstVpssGrpAttr->enPixelFormat = enum_num;
        }

        pstVpssGrpAttr->stFrameRate.s32SrcFrameRate = ini_getl(tmp_section, "src_framerate", 0, file);
        pstVpssGrpAttr->stFrameRate.s32DstFrameRate = ini_getl(tmp_section, "dst_framerate", 0, file);
        pstVpssGrpAttr->u8VpssDev                   = ini_getl(tmp_section, "vpss_dev", 0, file);
        pstVpssGrpAttr->u32MaxW                     = ini_getl(tmp_section, "max_w", 0, file);
        pstVpssGrpAttr->u32MaxH                     = ini_getl(tmp_section, "max_h", 0, file);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Group_ID_%d Config: pix_fmt=%2d sfr=%2d dfr=%2d Dev=%d W=%4d H=%4d\n", 
            grp_idx, pstVpssGrpAttr->enPixelFormat, pstVpssGrpAttr->stFrameRate.s32SrcFrameRate, pstVpssGrpAttr->stFrameRate.s32DstFrameRate,
            pstVpssGrpAttr->u8VpssDev, pstVpssGrpAttr->u32MaxW, pstVpssGrpAttr->u32MaxH);

        VPSS_CROP_INFO_S *pstVpssGrpCropInfo = &Vpss->astVpssGrpCfg[grp_idx].stVpssGrpCropInfo;
        pstVpssGrpCropInfo->bEnable = ini_getl(tmp_section, "crop_en", 0, file);
        if (pstVpssGrpCropInfo->bEnable){
            ini_gets(tmp_section, "crop_coor", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, vpss_crop_coordinate, VPSS_CROP_COORDINATE_MAX, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][crop_coor] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][crop_coor] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                pstVpssGrpCropInfo->enCropCoordinate = enum_num;
            }

            pstVpssGrpCropInfo->stCropRect.s32X      = ini_getl(tmp_section, "crop_rect_x", 0, file);
            pstVpssGrpCropInfo->stCropRect.s32Y      = ini_getl(tmp_section, "crop_rect_y", 0, file);
            pstVpssGrpCropInfo->stCropRect.u32Width  = ini_getl(tmp_section, "crop_rect_w", 0, file);
            pstVpssGrpCropInfo->stCropRect.u32Height = ini_getl(tmp_section, "crop_rect_h", 0, file);
        }

        CVI_U32 grp_chn_cnt = ini_getl(tmp_section, "chn_cnt", 0, file);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Group_ID_%d channel count = %d\n", grp_idx, grp_chn_cnt);
        /* load vpss group channel config */
        for (chn_idx = 0; chn_idx < grp_chn_cnt; chn_idx++) {
            memset(tmp_section, 0, sizeof(tmp_section));
            snprintf(tmp_section, sizeof(tmp_section), "vpssgrp%d.chn%d", grp_idx, chn_idx);
            Vpss->astVpssGrpCfg[grp_idx].abChnEnable[chn_idx] = ini_getl(tmp_section, "chn_enable", 0, file);
            if (!Vpss->astVpssGrpCfg[grp_idx].abChnEnable[chn_idx])
                continue;

            VPSS_CHN_ATTR_S *pastVpssChnAttr = &Vpss->astVpssGrpCfg[grp_idx].astVpssChnAttr[chn_idx];
            pastVpssChnAttr->u32Width = ini_getl(tmp_section, "width", 0, file);
            pastVpssChnAttr->u32Height = ini_getl(tmp_section, "height", 0, file);

            ini_gets(tmp_section, "video_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, video_format, VIDEO_FORMAT_MAX, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][video_fmt] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][video_fmt] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                pastVpssChnAttr->enVideoFormat = enum_num;
            }

            ini_gets(tmp_section, "chn_pixel_fmt", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, pixel_format, PIXEL_FORMAT_MAX, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][chn_pixel_fmt] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][chn_pixel_fmt] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                pastVpssChnAttr->enPixelFormat = enum_num;
            }

            pastVpssChnAttr->stFrameRate.s32SrcFrameRate = ini_getl(tmp_section, "src_framerate", 0, file);
            pastVpssChnAttr->stFrameRate.s32DstFrameRate = ini_getl(tmp_section, "dst_framerate", 0, file);
            pastVpssChnAttr->u32Depth                    = ini_getl(tmp_section, "depth", 0, file);
            pastVpssChnAttr->bMirror                     = ini_getl(tmp_section, "mirror", 0, file);
            pastVpssChnAttr->bFlip                       = ini_getl(tmp_section, "flip", 0, file);

            ini_gets(tmp_section, "aspectratio", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, aspect_ratio, ASPECT_RATIO_MAX, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][aspectratio] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][aspectratio] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                pastVpssChnAttr->stAspectRatio.enMode = enum_num;
            }

            if (pastVpssChnAttr->stAspectRatio.enMode == ASPECT_RATIO_MANUAL) { /*ASPECT_RATIO_MANUAL*/
                pastVpssChnAttr->stAspectRatio.stVideoRect.s32X      = ini_getl(tmp_section, "s32x", 0, file);
                pastVpssChnAttr->stAspectRatio.stVideoRect.s32Y      = ini_getl(tmp_section, "s32y", 0, file);
                pastVpssChnAttr->stAspectRatio.stVideoRect.u32Width  = ini_getl(tmp_section, "rec_width", 0, file);
                pastVpssChnAttr->stAspectRatio.stVideoRect.u32Height = ini_getl(tmp_section, "rec_heigh", 0, file);
                pastVpssChnAttr->stAspectRatio.bEnableBgColor        = ini_getl(tmp_section, "en_color", 0, file);
                if (pastVpssChnAttr->stAspectRatio.bEnableBgColor != 0)
                    pastVpssChnAttr->stAspectRatio.u32BgColor = ini_getl(tmp_section, "color", 0, file);
            }
            pastVpssChnAttr->stNormalize.bEnable = ini_getl(tmp_section, "normalize", 0, file);

            VPSS_CROP_INFO_S *pstVpssChnCropInfo = &Vpss->astVpssGrpCfg[grp_idx].stVpssChnCropInfo[chn_idx];
            pstVpssChnCropInfo->bEnable = ini_getl(tmp_section, "crop_en", 0, file);
            if (pstVpssChnCropInfo->bEnable) {
                ini_gets(tmp_section, "crop_coor", " ", str_name, PARAM_STRING_NAME_LEN, file);
                ret = Convert_StrName_to_EnumNum(str_name, vpss_crop_coordinate, VPSS_CROP_COORDINATE_MAX, &enum_num);
                if (ret != CVI_SUCCESS) {
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "Fail to convert string name [%s] to enum number!\n", str_name);
                } else {
                    APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][crop_coor] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                    pstVpssChnCropInfo->enCropCoordinate = enum_num;
                }

                pstVpssChnCropInfo->stCropRect.s32X      = ini_getl(tmp_section, "crop_rect_x", 0, file);
                pstVpssChnCropInfo->stCropRect.s32Y      = ini_getl(tmp_section, "crop_rect_y", 0, file);
                pstVpssChnCropInfo->stCropRect.u32Width  = ini_getl(tmp_section, "crop_rect_w", 0, file);
                pstVpssChnCropInfo->stCropRect.u32Height = ini_getl(tmp_section, "crop_rect_h", 0, file);
            }

            Vpss->astVpssGrpCfg[grp_idx].aAttachEn[chn_idx] = ini_getl(tmp_section, "attach_en", 0, file);
            if (Vpss->astVpssGrpCfg[grp_idx].aAttachEn[chn_idx]) {
                Vpss->astVpssGrpCfg[grp_idx].aAttachPool[chn_idx] = ini_getl(tmp_section, "attach_pool", 0, file);
            }
            APP_PROF_LOG_PRINT(LEVEL_INFO, "Chn_ID_%d config: sft=%2d dfr=%2d W=%4d H=%4d Depth=%d Mirror=%d Flip=%d V_fmt=%2d P_fmt=%2d\n", 
                chn_idx, pastVpssChnAttr->stFrameRate.s32SrcFrameRate, pastVpssChnAttr->stFrameRate.s32DstFrameRate,
                pastVpssChnAttr->u32Width, pastVpssChnAttr->u32Height, pastVpssChnAttr->u32Depth, pastVpssChnAttr->bMirror, pastVpssChnAttr->bFlip,
                pastVpssChnAttr->enVideoFormat, pastVpssChnAttr->enPixelFormat);
        }
    }
    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading vpss config ------------------> done \n\n");
    return CVI_SUCCESS;
}

static int _Load_Param_Venc(const char *file, APP_PARAM_VENC_CTX_S *Venc)
{
    int i = 0;
    long int chn_num = 0;
    int buffSize = 0;
    int enum_num = 0;
    int ret = 0;
    char tmp_section[32] = {0};
    char tmp_buff[PARAM_STRING_LEN] = {0};
    char str_name[PARAM_STRING_NAME_LEN] = {0};

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading venc config ------------------> start \n");

    chn_num = ini_getl("venc_config", "chn_num", 0, file);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "vpss_chn_num: %ld\n", chn_num);
    Venc->s32VencChnCnt = chn_num;

    for (i = 0; i < chn_num; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        snprintf(tmp_section, sizeof(tmp_section), "vencchn%d", i);
        Venc->astVencChnCfg[i].VencChn = i;

        Venc->astVencChnCfg[i].bEnable = ini_getl(tmp_section, "bEnable", 0, file);
        if (!Venc->astVencChnCfg[i].bEnable) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc_chn[%d] not enable!\n", i);
            continue;
        }

        ini_gets(tmp_section, "en_type", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, payload_type, PT_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][en_type] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][en_type] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Venc->astVencChnCfg[i].enType = enum_num;
        }

        Venc->astVencChnCfg[i].StreamTo  = ini_getl(tmp_section, "send_to", 0, file);
        Venc->astVencChnCfg[i].VpssGrp   = ini_getl(tmp_section, "vpss_grp", 0, file);
        Venc->astVencChnCfg[i].VpssChn   = ini_getl(tmp_section, "vpss_chn", 0, file);
        Venc->astVencChnCfg[i].u32Width  = ini_getl(tmp_section, "width", 0, file);
        Venc->astVencChnCfg[i].u32Height = ini_getl(tmp_section, "height", 0, file);

        // Venc->astVencChnCfg[i].enBindMode       = ini_getl(tmp_section, "bind_mode", 0, file);
        ini_gets(tmp_section, "bind_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, venc_bind_mode, VENC_BIND_MAX, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][bind_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][bind_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Venc->astVencChnCfg[i].enBindMode = enum_num;
        }


        if (Venc->astVencChnCfg[i].enBindMode != VENC_BIND_DISABLE) {
            ini_gets(tmp_section, "src_mod_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, mode_id, CVI_ID_BUTT, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][src_mod_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][src_mod_id] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                Venc->astVencChnCfg[i].astChn[0].enModId = enum_num;
            }

            Venc->astVencChnCfg[i].astChn[0].s32DevId = ini_getl(tmp_section, "src_dev_id", 0, file);
            Venc->astVencChnCfg[i].astChn[0].s32ChnId = ini_getl(tmp_section, "src_chn_id", 0, file);

            ini_gets(tmp_section, "dst_mod_id", " ", str_name, PARAM_STRING_NAME_LEN, file);
            ret = Convert_StrName_to_EnumNum(str_name, mode_id, CVI_ID_BUTT, &enum_num);
            if (ret != CVI_SUCCESS) {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dst_mod_id] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
            } else {
                APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][dst_mod_id] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
                Venc->astVencChnCfg[i].astChn[1].enModId = enum_num;
            }

            Venc->astVencChnCfg[i].astChn[1].s32DevId = ini_getl(tmp_section, "dst_dev_id", 0, file);
            Venc->astVencChnCfg[i].astChn[1].s32ChnId = ini_getl(tmp_section, "dst_chn_id", 0, file);
        }
        ini_gets(tmp_section, "save_path", " ", tmp_buff, PARAM_STRING_LEN, file);
        strncpy(Venc->astVencChnCfg[i].SavePath, tmp_buff, PARAM_STRING_LEN);

        APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc_chn[%d] enType=%d StreamTo=%d VpssGrp=%d VpssChn=%d u32Width=%d u32Height=%d\n save path =%s\n", 
            i, Venc->astVencChnCfg[i].enType, Venc->astVencChnCfg[i].StreamTo, Venc->astVencChnCfg[i].VpssGrp, Venc->astVencChnCfg[i].VpssChn, 
            Venc->astVencChnCfg[i].u32Width, Venc->astVencChnCfg[i].u32Height, Venc->astVencChnCfg[i].SavePath);

        ini_gets(tmp_section, "rc_mode", " ", str_name, PARAM_STRING_NAME_LEN, file);
        ret = Convert_StrName_to_EnumNum(str_name, venc_rc_mode, VENC_RC_MODE_BUTT, &enum_num);
        if (ret != CVI_SUCCESS) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][rc_mode] Fail to convert string name [%s] to enum number!\n", tmp_section, str_name);
        } else {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "[%s][rc_mode] Convert string name [%s] to enum number [%d].\n", tmp_section, str_name, enum_num);
            Venc->astVencChnCfg[i].enRcMode = enum_num;
        }

        buffSize = ini_getl(tmp_section, "bitStreamBuf", 0, file);
        Venc->astVencChnCfg[i].u32StreamBufSize = (buffSize << 10); 

        if (Venc->astVencChnCfg[i].enType != PT_JPEG) {

            Venc->astVencChnCfg[i].u32Profile      = ini_getl(tmp_section, "profile", 0, file);
            Venc->astVencChnCfg[i].u32SrcFrameRate = ini_getl(tmp_section, "src_framerate", 0, file);
            Venc->astVencChnCfg[i].u32DstFrameRate = ini_getl(tmp_section, "dst_framerate", 0, file);
            Venc->astVencChnCfg[i].enGopMode       = ini_getl(tmp_section, "gop_mode", 0, file);

            switch (Venc->astVencChnCfg[i].enGopMode) {
                case VENC_GOPMODE_NORMALP:
                    Venc->astVencChnCfg[i].unGopParam.stNormalP.s32IPQpDelta = ini_getl(tmp_section, "NormalP_IPQpDelta", 0, file);
                break;
                case VENC_GOPMODE_SMARTP:
                    Venc->astVencChnCfg[i].unGopParam.stSmartP.s32BgQpDelta  = ini_getl(tmp_section, "SmartP_BgQpDelta", 0, file);
                    Venc->astVencChnCfg[i].unGopParam.stSmartP.s32ViQpDelta  = ini_getl(tmp_section, "SmartP_ViQpDelta", 0, file);
                    Venc->astVencChnCfg[i].unGopParam.stSmartP.u32BgInterval = ini_getl(tmp_section, "SmartP_BgInterval", 0, file);
                break;
                case VENC_GOPMODE_DUALP:
                    Venc->astVencChnCfg[i].unGopParam.stDualP.s32IPQpDelta   = ini_getl(tmp_section, "DualP_IPQpDelta", 0, file);
                    Venc->astVencChnCfg[i].unGopParam.stDualP.s32SPQpDelta   = ini_getl(tmp_section, "DualP_SPQpDelta", 0, file);
                    Venc->astVencChnCfg[i].unGopParam.stDualP.u32SPInterval  = ini_getl(tmp_section, "DualP_SPInterval", 0, file);
                break;
                case VENC_GOPMODE_BIPREDB:
                    Venc->astVencChnCfg[i].unGopParam.stBipredB.s32BQpDelta  = ini_getl(tmp_section, "BipredB_BQpDelta", 0, file);
                    Venc->astVencChnCfg[i].unGopParam.stBipredB.s32IPQpDelta = ini_getl(tmp_section, "BipredB_IPQpDelta", 0, file);
                    Venc->astVencChnCfg[i].unGopParam.stBipredB.u32BFrmNum   = ini_getl(tmp_section, "BipredB_BFrmNum", 0, file);
                break;
                default:
                    APP_PROF_LOG_PRINT(LEVEL_ERROR, "venc_chn[%d] gop mode: %d invalid", i, Venc->astVencChnCfg[i].enGopMode);
                break;
            }

            Venc->astVencChnCfg[i].u32BitRate                     = ini_getl(tmp_section, "bit_rate", 0, file);
            Venc->astVencChnCfg[i].u32MaxBitRate                  = ini_getl(tmp_section, "max_bitrate", 0, file);
            Venc->astVencChnCfg[i].bSingleCore                    = ini_getl(tmp_section, "single_core", 0, file);
            Venc->astVencChnCfg[i].u32Gop                         = ini_getl(tmp_section, "gop", 0, file);
            Venc->astVencChnCfg[i].u32IQp                         = ini_getl(tmp_section, "fixed_IQP", 0, file);
            Venc->astVencChnCfg[i].u32PQp                         = ini_getl(tmp_section, "fixed_QPQ", 0, file);
            Venc->astVencChnCfg[i].statTime                       = ini_getl(tmp_section, "statTime", 0, file);
            Venc->astVencChnCfg[i].u32Duration                    = ini_getl(tmp_section, "file_duration", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32ThrdLv            = ini_getl(tmp_section, "ThrdLv", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32FirstFrameStartQp = ini_getl(tmp_section, "firstFrmstartQp", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32InitialDelay      = ini_getl(tmp_section, "initialDelay", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MaxIprop          = ini_getl(tmp_section, "MaxIprop", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MinIprop          = ini_getl(tmp_section, "MinIprop", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MaxQp             = ini_getl(tmp_section, "MaxQp", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MinQp             = ini_getl(tmp_section, "MinQp", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MaxIQp            = ini_getl(tmp_section, "MaxIQp", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MinIQp            = ini_getl(tmp_section, "MinIQp", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32ChangePos         = ini_getl(tmp_section, "ChangePos", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32MinStillPercent   = ini_getl(tmp_section, "MinStillPercent", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MaxStillQP        = ini_getl(tmp_section, "MaxStillQP", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MinStillPSNR      = ini_getl(tmp_section, "MinStillPSNR", 0, file);
            Venc->astVencChnCfg[i].stRcParam.u32MotionSensitivity = ini_getl(tmp_section, "MotionSensitivity", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32AvbrFrmLostOpen   = ini_getl(tmp_section, "AvbrFrmLostOpen", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32AvbrFrmGap        = ini_getl(tmp_section, "AvbrFrmGap", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32AvbrPureStillThr  = ini_getl(tmp_section, "AvbrPureStillThr", 0, file);
            Venc->astVencChnCfg[i].stRcParam.s32MaxReEncodeTimes  = ini_getl(tmp_section, "MaxReEncodeTimes", 0, file);

            APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc_chn[%d] enRcMode=%d u32BitRate=%d u32MaxBitRate=%d enBindMode=%d bSingleCore=%d\n", 
                i, Venc->astVencChnCfg[i].enRcMode, Venc->astVencChnCfg[i].u32BitRate, Venc->astVencChnCfg[i].u32MaxBitRate, 
                Venc->astVencChnCfg[i].enBindMode, Venc->astVencChnCfg[i].bSingleCore);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "u32Gop=%d statTime=%d u32ThrdLv=%d\n", 
                Venc->astVencChnCfg[i].u32Gop, Venc->astVencChnCfg[i].statTime, Venc->astVencChnCfg[i].stRcParam.u32ThrdLv);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "u32MaxQp=%d u32MinQp=%d u32MaxIQp=%d u32MinIQp=%d s32ChangePos=%d s32InitialDelay=%d\n", 
                Venc->astVencChnCfg[i].stRcParam.u32MaxQp, Venc->astVencChnCfg[i].stRcParam.u32MinQp, Venc->astVencChnCfg[i].stRcParam.u32MaxIQp, 
                Venc->astVencChnCfg[i].stRcParam.u32MinIQp, Venc->astVencChnCfg[i].stRcParam.s32ChangePos, Venc->astVencChnCfg[i].stRcParam.s32InitialDelay);
        } else {
            Venc->astVencChnCfg[i].stJpegCodecParam.quality   = ini_getl(tmp_section, "quality", 0, file);
            Venc->astVencChnCfg[i].stJpegCodecParam.MCUPerECS = ini_getl(tmp_section, "MCUPerECS", 0, file);
            APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc_chn[%d] quality=%d\n", i, Venc->astVencChnCfg[i].stJpegCodecParam.quality);
        }
    }

    int roi_num = ini_getl("roi_config", "max_num", 0, file);
    APP_PROF_LOG_PRINT(LEVEL_INFO, "roi_max_num: %d\n", roi_num);

    for (i = 0; i < roi_num; i++) {
        memset(tmp_section, 0, sizeof(tmp_section));
        sprintf(tmp_section, "roi_index%d", i);

        Venc->astRoiCfg[i].u32Index = i;
        Venc->astRoiCfg[i].bEnable = ini_getl(tmp_section, "bEnable", 0, file);
        if (!Venc->astRoiCfg[i].bEnable) {
            APP_PROF_LOG_PRINT(LEVEL_INFO, "Roi[%d] not enable!\n", i);
            continue;
        }

        Venc->astRoiCfg[i].VencChn   = ini_getl(tmp_section, "venc", 0, file);
        Venc->astRoiCfg[i].bAbsQp    = ini_getl(tmp_section, "absqp", 0, file);
        Venc->astRoiCfg[i].u32Qp     = ini_getl(tmp_section, "qp", 0, file);
        Venc->astRoiCfg[i].u32X      = ini_getl(tmp_section, "x", 0, file);
        Venc->astRoiCfg[i].u32Y      = ini_getl(tmp_section, "y", 0, file);
        Venc->astRoiCfg[i].u32Width  = ini_getl(tmp_section, "width", 0, file);
        Venc->astRoiCfg[i].u32Height = ini_getl(tmp_section, "height", 0, file);
        APP_PROF_LOG_PRINT(LEVEL_INFO, "Venc_chn[%d] bAbsQp=%d u32Qp=%d xy=(%d,%d) wd=(%d,%d)\n",
            Venc->astRoiCfg[i].VencChn, Venc->astRoiCfg[i].bAbsQp, Venc->astRoiCfg[i].u32Qp,
            Venc->astRoiCfg[i].u32X, Venc->astRoiCfg[i].u32Y, Venc->astRoiCfg[i].u32Width,
            Venc->astRoiCfg[i].u32Height);
    }

    APP_PROF_LOG_PRINT(LEVEL_INFO, "loading Roi config ------------------> done \n\n");

    return CVI_SUCCESS;
}

int app_ipcam_Param_Load(void)
{
    APP_CHK_RET(access(ParamCfgFile, F_OK), "param_config.ini access");

    APP_CHK_RET(_Load_Param_Sys(ParamCfgFile, app_ipcam_Sys_Param_Get()),     "load SysVb Param");
    APP_CHK_RET(_Load_Param_Vi(ParamCfgFile, app_ipcam_Vi_Param_Get()),       "load Vi Param");
    APP_CHK_RET(_Load_Param_Vpss(ParamCfgFile, app_ipcam_Vpss_Param_Get()),   "load Vpss Param");
    APP_CHK_RET(_Load_Param_Venc(ParamCfgFile, app_ipcam_Venc_Param_Get()),   "load Venc Param");

    return CVI_SUCCESS;
}
