set(SRCS "")
set(INCLUDE_DIRS "")
set(PRIVATE_INCLUDE_DIRS "")

function(project_register)
    set(options)
    set(one_value_args)
    set(multi_value_args SRCS INCLUDE_DIRS PRIVATE_INCLUDE_DIRS)
    
    cmake_parse_arguments(PROJECT_REGISTER "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    foreach(src IN LISTS PROJECT_REGISTER_SRCS)
        list(APPEND SRCS ${src})
    endforeach()
    
    foreach(inc IN LISTS PROJECT_REGISTER_INCLUDE_DIRS)
        list(APPEND INCLUDE_DIRS ${inc})
    endforeach()
    
    foreach(inc IN LISTS PROJECT_REGISTER_PRIVATE_INCLUDE_DIRS)
        list(APPEND PRIVATE_INCLUDE_DIRS ${inc})
    endforeach()
    
    set(SRCS ${SRCS} PARENT_SCOPE)
    set(INCLUDE_DIRS ${INCLUDE_DIRS} PARENT_SCOPE)
    set(PRIVATE_INCLUDE_DIRS ${PRIVATE_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()
