set(SG200X_PLATFORM ON)

include(${ROOT_DIR}/cmake/macro.cmake)

if( NOT "${SG200X_SDK_PATH}" STREQUAL "" )
message(STATUS "SG200X_SDK_PATH: ${SG200X_SDK_PATH}")

include_directories("${SG200X_SDK_PATH}/buildroot-2021.05/output/cvitek_CV181X_musl_riscv64/host/riscv64-buildroot-linux-musl/sysroot/usr/include")
link_directories("${SG200X_SDK_PATH}/buildroot-2021.05/output/cvitek_CV181X_musl_riscv64/host/riscv64-buildroot-linux-musl/sysroot/usr/lib")

include_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/include")
link_directories("${SG200X_SDK_PATH}/install/soc_sg2002_recamera_emmc/tpu_musl_riscv64/cvitek_tpu_sdk/lib")

else()
message(WARNING "SG200X_SDK_PATH is not set")
endif()

file(GLOB COMPONENTS LIST_DIRECTORIES  true ${ROOT_DIR}/components/*)

foreach(component IN LISTS COMPONENTS)
    if(EXISTS ${component}/CMakeLists.txt)
        include(${component}/CMakeLists.txt)
    endif()
endforeach()

include(${PROJECT_DIR}/main/CMakeLists.txt)

include(${ROOT_DIR}/cmake/build.cmake)

