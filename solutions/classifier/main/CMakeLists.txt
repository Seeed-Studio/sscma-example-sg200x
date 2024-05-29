component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE_REQUIREDS opencv_core opencv_imgcodecs opencv_imgproc cvikernel cviruntime cvimath z sscma
)
