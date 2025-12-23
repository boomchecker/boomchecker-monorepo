idf_component_register(
    SRCS "impulse_detection.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos log esp_system
)
