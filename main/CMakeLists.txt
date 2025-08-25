idf_component_register(SRCS "main.c" "logic_data.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_timer driver button nvs_flash)
