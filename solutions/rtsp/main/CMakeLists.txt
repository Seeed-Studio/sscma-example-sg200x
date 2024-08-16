component_register(
    COMPONENT_NAME main
    SRCS ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/H264FramedLiveSource.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/H264LiveVideoServerMediaSubsession.cpp
    SRCS ${CMAKE_CURRENT_LIST_DIR}/announceURL.cpp

    PRIVATE_REQUIREDS liveMedia groupsock BasicUsageEnvironment UsageEnvironment ssl crypto
)