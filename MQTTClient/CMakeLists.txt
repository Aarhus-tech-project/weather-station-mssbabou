cmake_minimum_required(VERSION 3.10)
project(MQTTClient C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- Paho (submodule) -----
add_subdirectory(external/paho.mqtt.c)

# ----- jsmn (header-only submodule) -----
# expect: external/jsmn/jsmn.h (NO .c file)
if(EXISTS ${CMAKE_SOURCE_DIR}/external/jsmn/jsmn.h)
  add_library(jsmn INTERFACE)
  target_include_directories(jsmn INTERFACE ${CMAKE_SOURCE_DIR}/external/jsmn)
  # Make jsmn functions static to avoid duplicate symbols
  target_compile_definitions(jsmn INTERFACE JSMN_STATIC)
else()
  message(FATAL_ERROR "jsmn.h not found at external/jsmn/jsmn.h")
endif()

# Sources
file(GLOB SOURCES "src/*.c")

# ----- PostgreSQL (libpq) -----
find_package(PostgreSQL REQUIRED)

# Executable
add_executable(MQTTClient ${SOURCES})

# Includes for your headers + libpq
target_include_directories(MQTTClient PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${PostgreSQL_INCLUDE_DIRS}
)

# Link deps (jsmn is header-only but we link to propagate INTERFACE defs/includes)
target_link_libraries(MQTTClient PRIVATE
  paho-mqtt3c
  PostgreSQL::PostgreSQL
  jsmn
)