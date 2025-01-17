#ifndef __APP_IPCAM_ISP_H__
#define __APP_IPCAM_ISP_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef SUPPORT_ISP_PQTOOL
#define PQ_BIN_SDR          "/mnt/data/cvi_sdr_bin"
#define PQ_BIN_IR           "/mnt/data/cvi_sdr_ir_bin"
#define PQ_BIN_NIGHT_COLOR  "/mnt/data/cvi_night_color_bin"
#define PQ_BIN_WDR          "/mnt/data/cvi_wdr_bin"
#else
#define PQ_BIN_SDR_PREFIX   "/mnt/cfg/param/cvi_sdr_bin"
#define PQ_BIN_IR           "/mnt/cfg/param/cvi_sdr_ir_bin"
#define PQ_BIN_NIGHT_COLOR  "/mnt/cfg/param/cvi_night_color_bin"
#define PQ_BIN_WDR          "/mnt/cfg/param/cvi_wdr_bin"

extern char* app_ipcam_Isp_pq_bin(void);
#define PQ_BIN_SDR   app_ipcam_Isp_pq_bin()
#endif

int app_ipcam_Vi_Isp_Init(void);
int app_ipcam_Vi_Isp_DeInit(void);
int app_ipcam_Vi_Isp_Start(void);
int app_ipcam_Vi_Isp_Stop(void);

#ifdef __cplusplus
}
#endif

#endif // __APP_IPCAM_ISP_H__
