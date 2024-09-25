configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)

target_link_libraries(${PROJECT_NAME} PRIVATE main)

