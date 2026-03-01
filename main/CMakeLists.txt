idf_component_register(
    SRCS "main.c" "bt_hfp.c" "espnow_comm.c" "group_mgr.c" "audio_pipe.c" "ui.c" "pairing.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash esp_wifi mbedtls driver esp_timer
)
