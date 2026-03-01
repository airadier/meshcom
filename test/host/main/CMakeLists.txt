set(MESHCOM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../main")

idf_component_register(
    SRCS
        "test_main.c"
        "test_group_mgr.c"
        "test_audio_pipe.c"
        "mocks.c"
        "${MESHCOM_SRC_DIR}/group_mgr.c"
        "${MESHCOM_SRC_DIR}/audio_pipe.c"
    INCLUDE_DIRS
        "."
        "${MESHCOM_SRC_DIR}"
    REQUIRES unity mbedtls nvs_flash esp_timer
)
