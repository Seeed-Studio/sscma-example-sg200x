set(AUDIO_PLUGIN_INC 
    "${CMAKE_CURRENT_LIST_DIR}/"
)

file(GLOB AUDIO_PLUGIN_SRC 
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

set(AUDIO_RREQIRDS )

include_directories(${AUDIO_PLUGIN_INC})

add_library(cvi_audio_plugin SHARED ${AUDIO_PLUGIN_SRC})

target_link_libraries(cvi_audio_plugin ${AUDIO_RREQIRDS})

set_target_properties(cvi_audio_plugin PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)
