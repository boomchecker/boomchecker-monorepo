file(GLOB_RECURSE WIFI_SOURCES "wifi/*.c")

idf_component_register(
        SRCS 
                "middleware.c"
                ${WIFI_SOURCES}
        INCLUDE_DIRS "include"
                "wifi/include"
        REQUIRES 
                esp_wifi 
                esp_event 
                esp_netif 
                nvs_flash
)
