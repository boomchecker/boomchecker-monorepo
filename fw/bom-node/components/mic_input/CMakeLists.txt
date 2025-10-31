idf_component_register(
    SRCS "mic_input.c" "ring_buffer.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos log esp_system
)
