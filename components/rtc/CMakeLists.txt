idf_component_register(
    SRCS "rtc_time.c" "rtc_bm8563.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
)
