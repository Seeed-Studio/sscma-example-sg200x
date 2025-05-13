option(OPTION_TLS "Select TLS implementation" MG_TLS_NONE)
set(TLS_OPTIONS MG_TLS_NONE MG_TLS_BUILTIN MG_TLS_OPENSSL MG_TLS_MBEDTLS)
set_property(CACHE OPTION_TLS PROPERTY STRINGS ${TLS_OPTIONS})

component_register(
    COMPONENT_NAME mongoose
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}"
    SRCS "${CMAKE_CURRENT_LIST_DIR}/mongoose.c"
)

if(OPTION_TLS STREQUAL "MG_TLS_BUILTIN")
    target_compile_definitions(mongoose PRIVATE -DMG_TLS=MG_TLS_BUILTIN)
elseif(OPTION_TLS STREQUAL "MG_TLS_OPENSSL")
    target_compile_definitions(mongoose PRIVATE -DMG_TLS=MG_TLS_OPENSSL)
elseif(OPTION_TLS STREQUAL "MG_TLS_MBEDTLS")
    target_compile_definitions(mongoose PRIVATE -DMG_TLS=MG_TLS_MBEDTLS)
endif()
