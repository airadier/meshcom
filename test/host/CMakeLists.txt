cmake_minimum_required(VERSION 3.16)
project(meshcom_host_test C)

# Paths
set(MESHCOM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../main)
set(IDF_PATH $ENV{IDF_PATH})

if(NOT IDF_PATH)
    # Fallback: check common location
    if(EXISTS "/tmp/esp-idf")
        set(IDF_PATH "/tmp/esp-idf")
    else()
        message(FATAL_ERROR "Set IDF_PATH to your ESP-IDF installation")
    endif()
endif()

set(MBEDTLS_DIR ${IDF_PATH}/components/mbedtls/mbedtls)
set(UNITY_DIR   ${IDF_PATH}/components/unity/unity/src)

# --- mbedTLS (just the crypto library) ---
# Build only mbedcrypto (no SSL, no X509)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
add_subdirectory(${MBEDTLS_DIR} mbedtls EXCLUDE_FROM_ALL)

# --- Unity test framework ---
add_library(unity STATIC ${UNITY_DIR}/unity.c)
target_include_directories(unity PUBLIC ${UNITY_DIR})

# --- ESP-IDF stubs (minimal types for host compilation) ---
add_library(esp_stubs INTERFACE)
target_include_directories(esp_stubs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/esp_stubs)

# --- Test executable ---
add_executable(meshcom_host_test
    main/test_main.c
    main/test_group_mgr.c
    main/test_audio_pipe.c
    main/mocks.c
    ${MESHCOM_SRC}/group_mgr.c
    ${MESHCOM_SRC}/audio_pipe.c
)

target_include_directories(meshcom_host_test PRIVATE
    main
    ${MESHCOM_SRC}
)

target_link_libraries(meshcom_host_test PRIVATE
    unity
    mbedcrypto
    esp_stubs
)

# Treat warnings but don't fail on them
target_compile_options(meshcom_host_test PRIVATE -Wall -Wextra -Wno-unused-parameter)
