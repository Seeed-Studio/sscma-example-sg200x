message(STATUS "Registering component cviruntime")

component_register(
    INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/include
    LIBRARIES opencv_core opencv_imgcodecs opencv_imgproc
    LIBRARY_DIRS ${CMAKE_CURRENT_LIST_DIR}/lib
)
