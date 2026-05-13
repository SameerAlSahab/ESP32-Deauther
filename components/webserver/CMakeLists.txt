idf_component_register(
    SRCS "webserver.c"
    INCLUDE_DIRS "interface"
    PRIV_REQUIRES
        hccapx_serializer
        pcap_serializer
        esp_http_server
        wifi_controller
        main
        spiffs
        nvs_flash
        freertos
)

spiffs_create_partition_image(storage "../../data" FLASH_IN_PROJECT)
