set(AUDIO_PLUGIN_INC
    "${CMAKE_CURRENT_LIST_DIR}/"
)

file(GLOB AUDIO_PLUGIN_SRC
    "${CMAKE_CURRENT_LIST_DIR}/cvi_ain.c"
    "${CMAKE_CURRENT_LIST_DIR}/plugin.c"
)

set(AUDIO_PLUGIN_RREQIRDS atomic pthread sys
    tinyalsa cvi_audio cvi_dnvqe cvi_ssp cvi_ssp2 cvi_vqe cvi_VoiceEngine cvi_RES1
    aacdec2 aacenc2 aaccomm2 aacsbrdec2 aacsbrenc2
)

include_directories(${AUDIO_PLUGIN_INC})

add_library(asound_module_pcm_cvi_audio SHARED ${AUDIO_PLUGIN_SRC})

target_link_libraries(asound_module_pcm_cvi_audio PUBLIC ${AUDIO_PLUGIN_RREQIRDS})

add_definitions(-DPIC)
set_target_properties(asound_module_pcm_cvi_audio PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(asound_module_pcm_cvi_audio PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)
install(TARGETS asound_module_pcm_cvi_audio DESTINATION /usr/lib/alsa-lib)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/asound.conf DESTINATION /etc/)
