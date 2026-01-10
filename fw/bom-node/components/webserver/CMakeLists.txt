# Glob all relevant source files
file(GLOB_RECURSE API_SOURCES "api/*.c" "api/*/*.c" "api/*/*/*.c")
file(GLOB_RECURSE WEB_SOURCES "*.c" "slre/*.c")

idf_component_register(
    SRCS 
        ${API_SOURCES}
        ${WEB_SOURCES}
        
    INCLUDE_DIRS 
        "include" 
        "slre"
        "api/include"
        
    REQUIRES 
        esp_wifi 
        nvs_flash 
        esp_http_server 
        esp_http_client 
        vfs 
        spiffs 
        json
        ota
        spiffs
        middleware
        audio_streamer
)

spiffs_create_partition_image(website ../../generated FLASH_IN_PROJECT)
