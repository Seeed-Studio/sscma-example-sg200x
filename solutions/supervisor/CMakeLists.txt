cmake_minimum_required(VERSION 3.10)

project(
    supervisor-go
    VERSION 1.0.0
    DESCRIPTION "Secure supervisor daemon written in Go"
    LANGUAGES NONE
)

# Options
option(SUPERVISOR_BUILD_WWW "Build web frontend" ON)
option(SUPERVISOR_CGO "Enable CGO for build" OFF)

# Find Go
find_program(GO_EXECUTABLE go REQUIRED)

# Get Go version
execute_process(
    COMMAND ${GO_EXECUTABLE} version
    OUTPUT_VARIABLE GO_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "Found Go: ${GO_VERSION}")

# Set build variables
set(SUPERVISOR_VERSION ${PROJECT_VERSION})
set(SUPERVISOR_BUILD_TIME "${CMAKE_CURRENT_TIMESTAMP}")

# Output directory
set(BUILD_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${BUILD_OUTPUT_DIR})

# Target architecture detection
if(NOT DEFINED TARGET_ARCH)
    set(TARGET_ARCH "riscv64")
endif()

if(NOT DEFINED TARGET_OS)
    set(TARGET_OS "linux")
endif()

# Build command
set(GO_BUILD_LDFLAGS "-s -w -X 'main.Version=${SUPERVISOR_VERSION}' -X 'main.BuildTime=${SUPERVISOR_BUILD_TIME}'")

if(SUPERVISOR_CGO)
    set(CGO_ENABLED "1")
    if(TARGET_ARCH STREQUAL "riscv64")
        set(CC "riscv64-linux-musl-gcc")
    endif()
else()
    set(CGO_ENABLED "0")
    set(CC "")
endif()

# Custom target to build supervisor
add_custom_target(supervisor ALL
    COMMAND ${CMAKE_COMMAND} -E env
        GOOS=${TARGET_OS}
        GOARCH=${TARGET_ARCH}
        CGO_ENABLED=${CGO_ENABLED}
        CC=${CC}
        ${GO_EXECUTABLE} build
        -ldflags "${GO_BUILD_LDFLAGS}"
        -o ${BUILD_OUTPUT_DIR}/supervisor
        ./cmd/supervisor
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building supervisor (${TARGET_OS}/${TARGET_ARCH})"
)

# Custom target to run tests
add_custom_target(supervisor-test
    COMMAND ${GO_EXECUTABLE} test -v ./...
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running supervisor tests"
)

# Custom target to format code
add_custom_target(supervisor-fmt
    COMMAND ${GO_EXECUTABLE} fmt ./...
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting supervisor code"
)

# Custom target to download dependencies
add_custom_target(supervisor-deps
    COMMAND ${GO_EXECUTABLE} mod download
    COMMAND ${GO_EXECUTABLE} mod tidy
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Downloading supervisor dependencies"
)

# Web frontend build (if enabled)
if(SUPERVISOR_BUILD_WWW)
    find_program(NPM_EXECUTABLE npm)
    if(NPM_EXECUTABLE)
        set(WWW_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/www)
        set(WWW_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rootfs/usr/share/supervisor/www)
        
        if(EXISTS ${WWW_SOURCE_DIR}/package.json)
            add_custom_target(supervisor-www
                COMMAND ${NPM_EXECUTABLE} install
                COMMAND ${NPM_EXECUTABLE} run build
                COMMAND ${CMAKE_COMMAND} -E make_directory ${WWW_OUTPUT_DIR}
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${WWW_SOURCE_DIR}/dist ${WWW_OUTPUT_DIR}
                WORKING_DIRECTORY ${WWW_SOURCE_DIR}
                COMMENT "Building supervisor web frontend"
            )
            add_dependencies(supervisor supervisor-www)
        endif()
    endif()
endif()

# Installation
install(PROGRAMS ${BUILD_OUTPUT_DIR}/supervisor
    DESTINATION ${CMAKE_INSTALL_PREFIX}/usr/local/bin
)

# Install rootfs files (scripts, config, etc.)
set(ROOTFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rootfs)
if(EXISTS ${ROOTFS_DIR})
    install(DIRECTORY ${ROOTFS_DIR}/
        DESTINATION ${CMAKE_INSTALL_PREFIX}
        USE_SOURCE_PERMISSIONS
    )
endif()

# CPack configuration for creating packages
set(CPACK_PACKAGE_NAME "supervisor-go")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "SSCMA")
set(CPACK_GENERATOR "TGZ")
include(CPack)

message(STATUS "supervisor-go configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Target: ${TARGET_OS}/${TARGET_ARCH}")
message(STATUS "  CGO: ${CGO_ENABLED}")
message(STATUS "  Build WWW: ${SUPERVISOR_BUILD_WWW}")
