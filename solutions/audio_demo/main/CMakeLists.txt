link_directories("${CMAKE_CURRENT_LIST_DIR}")

component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    # PRIVATE_REQUIREDS video cvi_rtsp
    PRIVATE_REQUIREDS audio

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/"
)
