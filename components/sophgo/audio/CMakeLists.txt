set(AUDIO_INC 
    "${CMAKE_CURRENT_LIST_DIR}/"
    "${CMAKE_CURRENT_LIST_DIR}/include/"
)

# file(GLOB AUDIO_SRC
#     "${CMAKE_CURRENT_LIST_DIR}/*.c"
#     "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
# )

set(AUDIO_RREQIRDS atomic pthread 
    tinyalsa cvi_audio cvi_dnvqe cvi_ssp cvi_ssp2 cvi_vqe cvi_VoiceEngine cvi_RES1
    aacdec2 aacenc2 aaccomm2 aacsbrdec2 aacsbrenc2
)

# set(SUPPORT_EXTERNAL_AAC yes)

# if(SUPPORT_EXTERNAL_AAC STREQUAL "yes")
#     add_definitions(-DSUPPORT_EXTERNAL_AAC)
# endif()

# add_definitions(-DCVI_MODIFIED)
