file(GLOB_RECURSE srcs ${CMAKE_CURRENT_LIST_DIR}/*.c ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

set(incs ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/node
)

message(STATUS "SOURCES: ${SOURCES}")

set(CMAKE_BUILD_TYPE Release)

#set(CMAKE_BUILD_TYPE Debug)


set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

component_register(
    COMPONENT_NAME main
    SRCS ${srcs}
    INCLUDE_DIRS ${incs}
    PRIVATE_REQUIREDS sscma-micro cvi_rtsp  
    REQUIREDS opencv_core opencv_imgcodecs opencv_imgproc
)


