

foreach(req IN LISTS REQUIREDS)
    include(${ROOT_DIR}/components/${req}/CMakeLists.txt)
endforeach()


foreach(library_dir IN LISTS LIBRARY_DIRS)
    link_directories(${library_dir})
endforeach( )

add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${PRIVATE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})


