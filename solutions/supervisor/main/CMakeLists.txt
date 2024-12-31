# file(GLOB_RECURSE srcs ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

set(incs ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

set(srcs 
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/app_daemon.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/api_device.cpp
)

component_register(
    COMPONENT_NAME main
    INCLUDE_DIRS ${incs}
    SRCS ${srcs}
    PRIVATE_REQUIREDS ssl crypto hv
)
