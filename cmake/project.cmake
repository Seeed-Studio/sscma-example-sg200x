set(SG200X_PLATFORM ON)

include(${ROOT_DIR}/cmake/toolchain-riscv64-linux-musl-x86_64.cmake)

include(${ROOT_DIR}/cmake/macro.cmake)

file(GLOB COMPONENTS LIST_DIRECTORIES  true ${ROOT_DIR}/components/*)

foreach(component IN LISTS COMPONENTS)
    if(EXISTS ${component}/CMakeLists.txt)
        include(${component}/CMakeLists.txt)
    endif()
endforeach()

include(${PROJECT_DIR}/main/CMakeLists.txt)

include(${ROOT_DIR}/cmake/build.cmake)
