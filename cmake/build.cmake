add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${PRIVATE_INCLUDE_DIRS})
