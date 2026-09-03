idf_component_register(SRCS "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__esp-tflite-micro esp_timer esp_system
                    PRIV_REQUIRES driver esp_driver_usb_serial_jtag)