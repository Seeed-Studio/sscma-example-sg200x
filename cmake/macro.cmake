set(REQUIREDS "")

function(component_register)
    set(options SHARED)
    set(one_value_args COMPONENT_NAME)
    set(multi_value_args SRCS INCLUDE_DIRS PRIVATE_INCLUDE_DIRS REQUIREDS PRIVATE_REQUIREDS LIBRARY_DIRS)

    cmake_parse_arguments(COMPONENT_REGISTER "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    message(STATUS "Registering component ${COMPONENT_REGISTER_COMPONENT_NAME}")
    
    # option(ENABLE_${COMPONENT_REGISTER_COMPONENT_NAME} "Enable ${COMPONENT_REGISTER_COMPONENT_NAME}" OFF)
        
    # if(NOT ENABLE_${COMPONENT_REGISTER_COMPONENT_NAME})
    #     return()
    # endif()

    # message(STATUS "Enable ${COMPONENT_REGISTER_COMPONENT_NAME} ON")

    foreach(lib_dir IN LISTS COMPONENT_REGISTER_LIBRARY_DIRS)
        message(STATUS "Linking ${lib_dir}")
        link_directories(${lib_dir})
    endforeach()
    
    foreach(inc IN LISTS COMPONENT_REGISTER_INCLUDE_DIRS)
        message(STATUS "Including ${inc}")
        include_directories(${inc})
    endforeach()


    if(DEFINED COMPONENT_REGISTER_SRCS)
        if(COMPONENT_REGISTER_SHARED)
            add_library(${COMPONENT_REGISTER_COMPONENT_NAME} SHARED ${COMPONENT_REGISTER_SRCS})
            install(TARGETS ${COMPONENT_REGISTER_COMPONENT_NAME} DESTINATION ${CMAKE_BINARY_DIR}/lib)
        else()
            add_library(${COMPONENT_REGISTER_COMPONENT_NAME} STATIC ${COMPONENT_REGISTER_SRCS})
        endif()
        set_target_properties(${COMPONENT_REGISTER_COMPONENT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_include_directories(${COMPONENT_REGISTER_COMPONENT_NAME} PRIVATE ${COMPONENT_REGISTER_PRIVATE_INCLUDE_DIRS})
        target_link_libraries(${COMPONENT_REGISTER_COMPONENT_NAME} PUBLIC ${COMPONENT_REGISTER_REQUIREDS})
        target_link_libraries(${COMPONENT_REGISTER_COMPONENT_NAME} PRIVATE ${COMPONENT_REGISTER_PRIVATE_REQUIREDS})
        list(APPEND REQUIREDS ${COMPONENT_REGISTER_REQUIREDS} ${COMPONENT_REGISTER_PRIVATE_REQUIREDS})
        set(REQUIREDS ${REQUIREDS} PARENT_SCOPE)
    endif()

    
endfunction()
