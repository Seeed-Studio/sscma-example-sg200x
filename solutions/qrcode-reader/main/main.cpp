#include <iostream>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <quirc.h>
#include <chrono>
#include <vector>
#include "rtsp_demo.h"
#include "video.h"

// Signal handler for program termination
static CVI_VOID app_ipcam_ExitSig_handle(CVI_S32 signo) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    if ((SIGINT == signo) || (SIGTERM == signo)) {
        deinitVideo();
        deinitRtsp();
        printf("ipcam receive a signal(%d) from terminate\n", signo);
    }

    exit(-1);
}
static int count = 0;
// Process QR code frame using Quirc
static int fpProcessQRCodeFrame(void* pData, void* pArgs, void* pUserData) {
    VIDEO_FRAME_INFO_S* VpssFrame = (VIDEO_FRAME_INFO_S*)pData;
    VIDEO_FRAME_S* f = &VpssFrame->stVFrame;

    count ++;
    if(count != 30){
        return 1;
    }else{
        count = 0;
    }
    
    // Map frame memory
    CVI_U8* pu8VirAddr = (CVI_U8*)CVI_SYS_Mmap(f->u64PhyAddr[0], f->u32Length[0]);
    if (!pu8VirAddr) {
        CVI_TRACE_LOG(CVI_DBG_ERR, "Failed to map frame memory\n");
        return CVI_FAILURE;
    }

    // Initialize Quirc
    struct quirc* qr = quirc_new();
    if (!qr) {
        CVI_SYS_Munmap(pu8VirAddr, f->u32Length[0]);
        CVI_TRACE_LOG(CVI_DBG_ERR, "Failed to initialize Quirc\n");
        return CVI_FAILURE;
    }

    // Resize Quirc buffer to match image dimensions
    if (quirc_resize(qr, f->u32Width, f->u32Height) < 0) {
        quirc_destroy(qr);
        CVI_SYS_Munmap(pu8VirAddr, f->u32Length[0]);
        CVI_TRACE_LOG(CVI_DBG_ERR, "Failed to resize Quirc buffer\n");
        return CVI_FAILURE;
    }

    // Convert RGB888 to grayscale (Y = 0.299R + 0.587G + 0.114B)
    std::vector<uint8_t> gray(f->u32Width * f->u32Height);
    for (uint32_t i = 0; i < f->u32Width * f->u32Height; ++i) {
        uint8_t r = pu8VirAddr[i * 3];
        uint8_t g = pu8VirAddr[i * 3 + 1];
        uint8_t b = pu8VirAddr[i * 3 + 2];
        gray[i] = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
    }

    // Perform decoding for 10 iterations to calculate average time
    std::vector<double> decode_times;
    std::vector<std::string> qr_codes;
    const int iterations = 10;

    for (int i = 0; i < iterations; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Copy grayscale data to Quirc buffer
        uint8_t* buffer = quirc_begin(qr, nullptr, nullptr);
        memcpy(buffer, gray.data(), f->u32Width * f->u32Height);
        quirc_end(qr);

        // Extract and decode QR codes (only store results from first iteration)
        if (i == 0) {
            int count = quirc_count(qr);
            qr_codes.clear();
            for (int j = 0; j < count; ++j) {
                struct quirc_code code;
                struct quirc_data data;
                quirc_extract(qr, j, &code);
                if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
                    qr_codes.push_back(std::string(reinterpret_cast<char*>(data.payload)));
                }
            }
        }

        // Record decoding time
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
        decode_times.push_back(elapsed.count());
    }

    // Calculate average decoding time
    double total_time = 0.0;
    for (double time : decode_times) {
        total_time += time;
    }
    double average_time_ms = decode_times.empty() ? 0.0 : total_time / decode_times.size();

    // Output results
    if (!qr_codes.empty()) {
        for (const auto& code : qr_codes) {
            printf("QR code text: %s, Average decode time: %.2f ms\n", code.c_str(), average_time_ms);
        }
    } else {
        APP_PROF_LOG_PRINT(LEVEL_DEBUG, "No QR code found, Average decode time: %.2f ms\n", average_time_ms);
    }

    // Clean up
    quirc_destroy(qr);
    CVI_SYS_Munmap(pu8VirAddr, f->u32Length[0]);

    return CVI_SUCCESS;
}

// Save encoded video frame (unchanged)
static int fpSaveVencFrame(void* pData, void* pArgs, void* pUserData) {
    VENC_STREAM_S* pstStream = (VENC_STREAM_S*)pData;
    APP_DATA_CTX_S* pstDataCtx = (APP_DATA_CTX_S*)pArgs;
    APP_DATA_PARAM_S* pstDataParam = &pstDataCtx->stDataParam;
    APP_VENC_CHN_CFG_S* pstVencChnCfg = (APP_VENC_CHN_CFG_S*)pstDataParam->pParam;

    static uint32_t count = 0;
    {
        if (pstVencChnCfg->pFile == NULL) {
            char szFilePath[100] = {0};
            sprintf(szFilePath,
                    "/userdata/local/VENC%d_%d%s",
                    pstVencChnCfg->VencChn,
                    count++,
                    app_ipcam_Postfix_Get(pstVencChnCfg->enType));
            count %= 10;
            printf("start save file to %s\n", szFilePath);
            pstVencChnCfg->pFile = fopen(szFilePath, "wb");
            if (pstVencChnCfg->pFile == NULL) {
                APP_PROF_LOG_PRINT(LEVEL_ERROR, "open file err, %s\n", szFilePath);
                return CVI_FAILURE;
            }
        }

        VENC_PACK_S* ppack;
        if ((pstVencChnCfg->enType == PT_H264) || (pstVencChnCfg->enType == PT_H265)) {
            if (pstVencChnCfg->fileNum == 0 && pstStream->u32PackCount < 2) {
                printf("skip first I-frame\n");
                return CVI_SUCCESS;
            }
        }
        for (CVI_U32 i = 0; i < pstStream->u32PackCount; i++) {
            ppack = &pstStream->pstPack[i];
            fwrite(ppack->pu8Addr + ppack->u32Offset,
                   ppack->u32Len - ppack->u32Offset,
                   1,
                   pstVencChnCfg->pFile);

            printf(
                "pack[%d], PTS = %lu, Addr = %p, Len = 0x%X, Offset = 0x%X DataType=%d\n",
                i,
                ppack->u64PTS,
                ppack->pu8Addr,
                ppack->u32Len,
                ppack->u32Offset,
                ppack->DataType.enH265EType);
        }

        if (pstVencChnCfg->enType == PT_JPEG) {
            fclose(pstVencChnCfg->pFile);
            pstVencChnCfg->pFile = NULL;
            printf("End save! \n");
        } else {
            if (++pstVencChnCfg->fileNum > pstVencChnCfg->u32Duration) {
                pstVencChnCfg->fileNum = 0;
                fclose(pstVencChnCfg->pFile);
                pstVencChnCfg->pFile = NULL;
                printf("End save! \n");
            }
        }
    }

    return CVI_SUCCESS;
}

// Main function
int main(int argc, char* argv[]) {
    signal(SIGINT, app_ipcam_ExitSig_handle);
    signal(SIGTERM, app_ipcam_ExitSig_handle);

    if (initVideo())
        return -1;

    video_ch_param_t param;

    // ch0: RGB for QR code detection
    param.format = VIDEO_FORMAT_RGB888;
    param.width  = 640;
    param.height = 640;
    param.fps    = 1;
    setupVideo(VIDEO_CH0, &param);
    registerVideoFrameHandler(VIDEO_CH0, 0, fpProcessQRCodeFrame, (void*)".rgb");

    // ch2: H264 with RTSP
    param.format = VIDEO_FORMAT_H264;
    param.width  = 1920;
    param.height = 1080;
    param.fps    = 30;
    setupVideo(VIDEO_CH2, &param);
    registerVideoFrameHandler(VIDEO_CH2, 0, fpStreamingSendToRtsp, NULL);
    initRtsp((0x01 << VIDEO_CH2));

    startVideo();

    while (1) {
        sleep(1);
    }

    return 0;
}