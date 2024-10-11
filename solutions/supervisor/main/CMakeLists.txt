component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/src/http.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/src/daemon.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/src/utils_device.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/src/utils_file.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/src/utils_user.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/src/utils_wifi.cpp
    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE_REQUIREDS ssl crypto hv
)
