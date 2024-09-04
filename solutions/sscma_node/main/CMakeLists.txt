file(GLOB_RECURSE srcs ${CMAKE_CURRENT_LIST_DIR}/*.c ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
set(incs ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/node
    ${CMAKE_CURRENT_LIST_DIR}/rtsp
)
message(STATUS "SOURCES: ${SOURCES}")
component_register(
    COMPONENT_NAME main
    SRCS ${srcs}
    INCLUDE_DIRS ${incs}
    PRIVATE_REQUIREDS sscma  cvi_rtsp
)
