set(VIDEO_INC 
    "${CMAKE_CURRENT_LIST_DIR}/"
    "${CMAKE_CURRENT_LIST_DIR}/include/"
)

set(VIDEO_SRC
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_comm.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_isp.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_ll.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_paramparse.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_sensors.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_sys.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_venc.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_vi.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_vpss.c"
    "${CMAKE_CURRENT_LIST_DIR}/video.c"
)

set(VIDEO_RREQIRDS ae af atomic awb cvi_bin cvi_bin_isp gdc isp isp_algo sys venc vpss vi vo
    cvi_ispd2 json-c sns_ov5647 sns_sc530ai_2l
)
