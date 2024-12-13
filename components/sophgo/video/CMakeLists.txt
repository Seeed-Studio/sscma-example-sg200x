set(VIDEO_INC 
    "${CMAKE_CURRENT_LIST_DIR}/"
    "${CMAKE_CURRENT_LIST_DIR}/include/"
)

file(GLOB VIDEO_SRC 
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
)

set(VIDEO_RREQIRDS ae af atomic awb cvi_bin cvi_bin_isp gdc isp isp_algo sys venc vpss vi vo
    cvi_ispd2 json-c sns_ov5647 sns_sc530ai_2l
)
