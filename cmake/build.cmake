
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dummy.c "")

add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_BINARY_DIR}/dummy.c)

target_link_libraries(${PROJECT_NAME} PRIVATE main)


