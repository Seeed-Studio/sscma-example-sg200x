set(AUDIO_PLUGIN_INC 
    "${CMAKE_CURRENT_LIST_DIR}/"
)

file(GLOB AUDIO_PLUGIN_SRC 
    "${CMAKE_CURRENT_LIST_DIR}/*.c"
)

set(AUDIO_RREQIRDS atomic pthread 
    tinyalsa cvi_audio cvi_dnvqe cvi_ssp cvi_ssp2 cvi_vqe cvi_VoiceEngine cvi_RES1
    aacdec2 aacenc2 aaccomm2 aacsbrdec2 aacsbrenc2
)

include_directories(${AUDIO_PLUGIN_INC})

add_library(alsa_plugin SHARED ${AUDIO_PLUGIN_SRC})

target_link_libraries(alsa_plugin ${ALSA_LIBRARIES})

set_target_properties(alsa_plugin PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)
