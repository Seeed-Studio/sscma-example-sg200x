link_directories("${CMAKE_CURRENT_LIST_DIR}")

component_register(
    COMPONENT_NAME main
    PRIVATE_REQUIREDS sophgo

    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
        ${ROOT_DIR}/components/sophgo/audio_plugin/cvi_aio.c

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/"
        ${ROOT_DIR}/components/sophgo/audio_plugin/
)
