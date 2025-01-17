set(AUDIO_PLUGIN_INC 
    "${CMAKE_CURRENT_LIST_DIR}/"
)

file(GLOB AUDIO_PLUGIN_SRC 
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

# set(AUDIO_RREQIRDS )

include_directories(${AUDIO_PLUGIN_INC})

add_library(asound_module_pcm_cvi_audio SHARED ${AUDIO_PLUGIN_SRC})

# target_link_libraries(asound_module_pcm_cvi_audio ${AUDIO_RREQIRDS})

# set_target_properties(asound_module_pcm_cvi_audio PROPERTIES POSITION_INDEPENDENT_CODE OFF)
set_target_properties(asound_module_pcm_cvi_audio PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)
