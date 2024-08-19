#ifndef __APP_IPCAM_PARAM_PARSE_H__
#define __APP_IPCAM_PARAM_PARSE_H__

#include "linux/cvi_common.h"
#include "linux/cvi_comm_video.h"
#include "app_ipcam_comm.h"
#include "app_ipcam_sys.h"
#include "app_ipcam_vi.h"
#include "app_ipcam_vpss.h"
#include "app_ipcam_venc.h"

#ifdef __cplusplus
extern "C"
{
#endif

int app_ipcam_Param_setVencChnType(int ch, PAYLOAD_TYPE_E enType);
int app_ipcam_Param_Load(void);

#ifdef __cplusplus
}
#endif

#endif
