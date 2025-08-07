cmake_minimum_required(VERSION 3.10)
project(MQTTClient C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add subdirectory to build Paho
add_subdirectory(external/paho.mqtt.c)

# Include headers
include_directories(include)

# Collect all source files
file(GLOB SOURCES "src/*.c")

# Create executable
add_executable(MQTTClient ${SOURCES})

# Link against Paho MQTT C lib (static)
target_link_libraries(MQTTClient paho-mqtt3c)
