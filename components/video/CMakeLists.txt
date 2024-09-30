include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/chip/cv181x/uapi/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/common/uapi/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/common/uapi/linux/")

include_directories("${SG200X_SDK_PATH}/cvi_mpi/include")
include_directories("${SG200X_SDK_PATH}/cvi_mpi/modules/isp/cv181x/isp-daemon2/inc/")
include_directories("${SG200X_SDK_PATH}/cvi_mpi/modules/isp/include/cv181x/")

link_directories("${SG200X_SDK_PATH}/cvi_mpi/lib/")

set(SENSOR_INC 
    "${CMAKE_CURRENT_LIST_DIR}/include/"
)

set(SENSOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_comm.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_isp.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_ll.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_paramparse.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_sensors.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_sys.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_venc.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_vi.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_vpss.c"
)

component_register(
    COMPONENT_NAME video
    PRIVATE_REQUIREDS ae af atomic awb cvi_bin cvi_bin_isp gdc isp isp_algo sys venc vpss vi vo sns_ov5647 sns_sc530ai_2l
    PRIVATE_REQUIREDS cvi_ispd2 json-c

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/"
    SRCS "${CMAKE_CURRENT_LIST_DIR}/video.c"

    INCLUDE_DIRS ${SENSOR_INC}
    SRCS ${SENSOR_SRC}
)
