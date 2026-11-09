file(GLOB HA_MQTT_SRCS "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")

component_register(
    COMPONENT_NAME ha_mqtt
    SRCS ${HA_MQTT_SRCS}
    INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/include"
    PRIVATE_REQUIREDS mosquitto pthread
)
