include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/chip/cv181x/uapi/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/common/uapi/")
include_directories("${SG200X_SDK_PATH}/osdrv/interdrv/v2/include/common/uapi/linux/")

include_directories("${SG200X_SDK_PATH}/cvi_mpi/include")
include_directories("${SG200X_SDK_PATH}/cvi_mpi/modules/isp/cv181x/isp-daemon2/inc/")
include_directories("${SG200X_SDK_PATH}/cvi_mpi/modules/isp/include/cv181x/")

link_directories("${SG200X_SDK_PATH}/cvi_mpi/lib/")

include(${CMAKE_CURRENT_LIST_DIR}/audio/CMakeLists.txt)
include(${CMAKE_CURRENT_LIST_DIR}/video/CMakeLists.txt)

option(ENABLE_AUDIO_PLUGIN "Build audio plugin" OFF)

if(ENABLE_AUDIO_PLUGIN)
    include(${CMAKE_CURRENT_LIST_DIR}/audio_plugin/CMakeLists.txt)
endif()

file(GLOB COMMON_SRC "${CMAKE_CURRENT_LIST_DIR}/common/*.c")

component_register(
    COMPONENT_NAME sophgo
    PRIVATE_REQUIREDS ${AUDIO_RREQIRDS} ${VIDEO_RREQIRDS}

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/common"
    INCLUDE_DIRS ${AUDIO_INC} ${VIDEO_INC}

    SRCS ${COMMON_SRC} ${AUDIO_SRC} ${VIDEO_SRC}
)
