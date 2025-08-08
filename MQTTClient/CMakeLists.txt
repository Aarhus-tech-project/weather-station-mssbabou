cmake_minimum_required(VERSION 3.10)
project(MQTTClient C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- Paho (submodule) -----
# expect: external/paho.mqtt.c (git submodule)
add_subdirectory(external/paho.mqtt.c)

# ----- jsmn (header-only submodule) -----
# expect: external/jsmn/jsmn.h (NO .c file)
if(EXISTS ${CMAKE_SOURCE_DIR}/external/jsmn/jsmn.h)
  add_library(jsmn INTERFACE)
  target_include_directories(jsmn INTERFACE ${CMAKE_SOURCE_DIR}/external/jsmn)
  # If your single-header needs a define to emit implementations, add it here:
  # target_compile_definitions(jsmn INTERFACE JSMN_STATIC)
else()
  message(FATAL_ERROR "jsmn.h not found at external/jsmn/jsmn.h")
endif()

# Your headers
include_directories(include)

# Sources
file(GLOB SOURCES "src/*.c")

# ----- PostgreSQL (libpq) -----
find_package(PostgreSQL REQUIRED)

# Executable
add_executable(MQTTClient ${SOURCES})

# Ensure libpq include dir is available
if (PostgreSQL_INCLUDE_DIRS)
  target_include_directories(MQTTClient PRIVATE ${PostgreSQL_INCLUDE_DIRS})
elseif (PostgreSQL_INCLUDE_DIR)
  target_include_directories(MQTTClient PRIVATE ${PostgreSQL_INCLUDE_DIR})
endif()

# Link Paho + libpq + jsmn (header-only)
target_link_libraries(MQTTClient
  PRIVATE
    paho-mqtt3c
    PostgreSQL::PostgreSQL
    jsmn
)