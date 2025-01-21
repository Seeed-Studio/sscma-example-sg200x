set(SOPHGO_PLATFORM ON)

include(${ROOT_DIR}/cmake/macro.cmake)

set(CMAKE_CXX_STANDARD 17)

if(NOT "${SG200X_SDK_PATH}" STREQUAL "")
    message(STATUS "SG200X_SDK_PATH: ${SG200X_SDK_PATH}")

    if("${SYSROOT}" STREQUAL "")
        set(SYSROOT ${SG200X_SDK_PATH}/buildroot-2021.05/output/cvitek_CV181X_musl_riscv64/host/riscv64-buildroot-linux-musl/sysroot)
    endif()

    message(STATUS "SYSROOT: ${SYSROOT}")

    include_directories("${SYSROOT}/usr/include")
    link_directories("${SYSROOT}/usr/lib")

    include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/include")
    link_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/lib")
    link_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/rootfs/mnt/system/lib")

    # rtsp
    include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_ai_sdk/sample/3rd/rtsp/include/cvi_rtsp")
    link_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_ai_sdk/sample/3rd/rtsp/lib")

    include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/include/liveMedia")
    include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/include/groupsock")
    include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/include/UsageEnvironment")
    include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/include/BasicUsageEnvironment")

else()
    message(WARNING "SG200X_SDK_PATH is not set")
endif()

file(GLOB COMPONENTS LIST_DIRECTORIES true ${ROOT_DIR}/components/*)
include(${PROJECT_DIR}/main/CMakeLists.txt)

set(SKIP_COMPONENTS "")

foreach(component IN LISTS COMPONENTS)
    get_filename_component(component_name ${component} NAME)
    message(STATUS "component: ${component_name}")

    if(EXISTS "${component}/CMakeLists.txt" AND component_name IN_LIST REQUIREDS)
        include("${component}/CMakeLists.txt")
    else()
        list(APPEND SKIP_COMPONENTS ${component})
    endif()

    foreach(component IN LISTS SKIP_COMPONENTS)
        get_filename_component(component_name ${component} NAME)

        if(EXISTS "${component}/CMakeLists.txt" AND component_name IN_LIST REQUIREDS)
            include("${component}/CMakeLists.txt")
        endif()
    endforeach()
endforeach()

include(${ROOT_DIR}/cmake/build.cmake)

include(${ROOT_DIR}/cmake/package.cmake)
