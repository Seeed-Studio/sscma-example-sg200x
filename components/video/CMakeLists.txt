include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/common/uapi/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/chip/cv181x/uapi/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/cvi_vc_drv/include/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/common/kapi/")

include_directories("${SG200X_SDK_PATH}/middleware/v2/include/")
include_directories("${SG200X_SDK_PATH}/middleware/v2/modules/isp/cv181x/isp/inc/")
include_directories("${SG200X_SDK_PATH}/middleware/v2/modules/isp/cv181x/isp-daemon2/inc/")
include_directories("${SG200X_SDK_PATH}/middleware/v2/modules/isp/include/cv181x/")

link_directories("${SG200X_SDK_PATH}/middleware/v2/lib/")

set(SENSOR_INC 
    "${CMAKE_CURRENT_LIST_DIR}/include/"
)

set(SENSOR_SRC
    "${CMAKE_CURRENT_LIST_DIR}/src/app_ipcam_comm.c"
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
    PRIVATE_REQUIREDS -lcvi_bin -lcvi_bin_isp -lvpu -lvenc 
        -lvdec -losdc -lsns_full -lisp -lawb -lae -laf 
        -lisp_algo -lsys -latomic

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/"
    SRCS "${CMAKE_CURRENT_LIST_DIR}/video.c"

    INCLUDE_DIRS ${SENSOR_INC}
    SRCS ${SENSOR_SRC}
)
