#ifndef __SOPHGO_VIDEO_PARAM_PARSE_H__
#define __SOPHGO_VIDEO_PARAM_PARSE_H__

#include <linux/cvi_comm_sys.h>

#include "app_ipcam_comm.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_venc.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_ipcam_Param_setVencChnType(int ch, PAYLOAD_TYPE_E enType);
int app_ipcam_Param_Load(void);

#ifdef __cplusplus
}
#endif

#endif // __SOPHGO_VIDEO_PARAM_PARSE_H__
