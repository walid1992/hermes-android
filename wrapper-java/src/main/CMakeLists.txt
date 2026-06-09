cmake_minimum_required(VERSION 3.22.1)
project("hermes-wrapper")

# Hermes headers from the engine module
# Users should set HERMES_INCLUDE_DIR to point to Hermes source headers
set(HERMES_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../hermes-engine/src/main/jniLibs" 
    CACHE PATH "Path to Hermes include directory")

add_library(hermes_wrapper SHARED
    cpp/hermes_context_jni.cpp
)

# Link against the prebuilt Hermes shared library
# The .so file is loaded at runtime via System.loadLibrary("hermes")
# Here we just need the headers for compilation
target_include_directories(hermes_wrapper PRIVATE
    ${HERMES_INCLUDE_DIR}
)

find_library(log-lib log)
target_link_libraries(hermes_wrapper
    ${log-lib}
)
