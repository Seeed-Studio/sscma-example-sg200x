component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    REQUIREDS asound avformat avcodec avutil
)
