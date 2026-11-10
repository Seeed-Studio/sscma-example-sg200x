file(GLOB_RECURSE SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/*.c 
    ${CMAKE_CURRENT_LIST_DIR}/common/*.c 
    ${CMAKE_CURRENT_LIST_DIR}/aruco/*.c
)

component_register(
    COMPONENT_NAME apriltag
    INCLUDE_DIRS 
        "${CMAKE_CURRENT_LIST_DIR}" 
        "${CMAKE_CURRENT_LIST_DIR}/common" 
        "${CMAKE_CURRENT_LIST_DIR}/aruco"
    SRCS "${SOURCES}"
)
