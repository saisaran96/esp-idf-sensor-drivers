idf_component_register(
        SRCS
        "rc522.c"
        INCLUDE_DIRS "include"
        REQUIRES
        driver
        hal_spi
)