component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE_REQUIREDS cviruntime sscma opencv_core opencv_imgcodecs opencv_imgproc hv
)
