file(GLOB_RECURSE srcs ${CMAKE_CURRENT_LIST_DIR}/*.c ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

set(incs ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

component_register(
    COMPONENT_NAME main
    SRCS ${srcs}
    INCLUDE_DIRS ${incs}
    PRIVATE_REQUIREDS ssl crypto hv
)