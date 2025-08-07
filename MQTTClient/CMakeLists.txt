cmake_minimum_required(VERSION 3.10)
project(MQTTClient C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include headers
include_directories(include)

# Collect all source files
file(GLOB SOURCES "src/*.c")

# Create executable
add_executable(MQTTClient ${SOURCES})
