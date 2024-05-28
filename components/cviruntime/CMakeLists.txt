message(STATUS "Registering component cviruntime")

component_register(
    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
    LIBRARIES cvikernel cviruntime cvimath z
    LIBRARY_DIRS ${CMAKE_CURRENT_LIST_DIR}/lib
)




