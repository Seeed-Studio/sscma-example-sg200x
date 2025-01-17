link_directories("${CMAKE_CURRENT_LIST_DIR}")

component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    PRIVATE_REQUIREDS sophgo

    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/"
)
