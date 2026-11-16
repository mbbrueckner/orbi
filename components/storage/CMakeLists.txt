idf_component_register(
    SRCS "storage_record.c" "storage_nvs.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash pet_core
)
