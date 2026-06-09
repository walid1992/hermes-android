cmake_minimum_required(VERSION 3.22.1)
project("hermes-wrapper")

# Include directories for Hermes and JSI headers
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cpp/include")

add_library(hermes_wrapper SHARED
    cpp/hermes_context_jni.cpp
)

target_include_directories(hermes_wrapper PRIVATE
    ${INCLUDE_DIR}
)

# Find prebuilt shared libraries from the hermes-engine module's jniLibs
# These are loaded at runtime via System.loadLibrary
set(HERMES_JNILIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../hermes-engine/src/main/jniLibs/${ANDROID_ABI}")

# Import prebuilt libhermes.so
add_library(hermes SHARED IMPORTED)
set_target_properties(hermes PROPERTIES
    IMPORTED_LOCATION "${HERMES_JNILIBS_DIR}/libhermes.so"
)

# Import prebuilt libjsi.so
add_library(jsi SHARED IMPORTED)
set_target_properties(jsi PROPERTIES
    IMPORTED_LOCATION "${HERMES_JNILIBS_DIR}/libjsi.so"
)

find_library(log-lib log)

target_link_libraries(hermes_wrapper
    hermes
    jsi
    ${log-lib}
)
