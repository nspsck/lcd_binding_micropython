
# Create an INTERFACE library for our C module.
add_library(usermod_lcd INTERFACE)

# user configure
set(set CONFIG_LCD_BUS_QSPI 1)
set(set CONFIG_LCD_DRIVER_RM67162 1)

# hal layer
set(HAL_DIR ${CMAKE_CURRENT_LIST_DIR}/hal)
set(ESP32_HAL_SRC ${HAL_DIR}/esp32/esp32.c)
set(ESP32_HAL_INC ${HAL_DIR}/esp32)
set(COMMON_HAL_INC ${HAL_DIR}/common)

# bus layer
set(BUS_DIR ${CMAKE_CURRENT_LIST_DIR}/bus)
set(COMMON_BUS_INC ${BUS_DIR}/common)
set(QSPI_BUS_SRC ${BUS_DIR}/qspi/qspi_panel.c)
set(QSPI_BUS_INC ${BUS_DIR}/qspi)

# driver layer
set(DRIVER_DIR ${CMAKE_CURRENT_LIST_DIR}/driver)
set(DRIVER_COMMON_SRC ${DRIVER_DIR}/common/lcd_panel_types.c)
set(DRIVER_COMMON_INC ${DRIVER_DIR}/common)
set(RM67162_DRIVER_SRC ${DRIVER_DIR}/rm67162/rm67162.c)
set(RM67162_DRIVER_INC ${DRIVER_DIR}/rm67162)

# Add our source files to the lib
set(SRC ${CMAKE_CURRENT_LIST_DIR}/modlcd.c)
LIST(APPEND SRC ${ESP32_HAL_SRC})
LIST(APPEND SRC ${COMMON_HAL_SRC})
LIST(APPEND SRC ${QSPI_BUS_SRC})
LIST(APPEND SRC ${DRIVER_COMMON_SRC})
LIST(APPEND SRC ${RM67162_DRIVER_SRC})

set(INC ${CMAKE_CURRENT_LIST_DIR} ${HAL_DIR}/esp32)
LIST(APPEND INC ${ESP32_HAL_INC})
LIST(APPEND INC ${COMMON_HAL_INC})
LIST(APPEND INC ${COMMON_BUS_INC})
LIST(APPEND INC ${QSPI_BUS_INC})
LIST(APPEND INC ${DRIVER_COMMON_INC})
LIST(APPEND INC ${RM67162_DRIVER_INC})

if (CONFIG_IDF_TARGET_ESP32 OR CONFIG_IDF_TARGET_ESP32S3)
    target_compile_definitions(usermod_lcd INTERFACE USE_ESP_LCD=1)
    if (CONFIG_IDF_TARGET_ESP32S3)
        LIST(APPEND SRC ${DPI_BUS_SRC})
        LIST(APPEND INC ${DPI_BUS_INC})
        target_compile_definitions(usermod_lcd INTERFACE DPI_LCD_SUPPORTED=1)
    endif()
endif()

target_sources(usermod_lcd INTERFACE ${SRC})
target_compile_options(usermod_lcd INTERFACE "-g")
# Add the current directory as an include directory.
target_include_directories(usermod_lcd INTERFACE ${INC})

# Link our INTERFACE library to the usermod target.
target_link_libraries(usermod INTERFACE usermod_lcd)
