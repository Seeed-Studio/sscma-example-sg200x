link_directories("${CMAKE_CURRENT_LIST_DIR}")

component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    PRIVATE_REQUIREDS cvi_rtsp sophgo

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/" "${CMAKE_CURRENT_LIST_DIR}/include"
    SRCS "${CMAKE_CURRENT_LIST_DIR}/stream_porting.c" "${CMAKE_CURRENT_LIST_DIR}/cvi_streamer.cpp" 
)
