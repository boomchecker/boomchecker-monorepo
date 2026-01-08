file(GLOB_RECURSE MEDIAN_SOURCES "median-detector/*.c")

idf_component_register(
        SRCS 
                "detector.c"
                ${MEDIAN_SOURCES}
        INCLUDE_DIRS 
                "include"
                "median-detector/include"
        REQUIRES
            driver
            freertos
            log
            esp_system
            mic_input 
)
