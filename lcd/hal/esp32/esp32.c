#include "esp32.h"

#include "qspi_panel.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#include "machine_hw_spi.c"
#include "py/runtime.h"

#define DEBUG_printf(...) // mp_printf(&mp_plat_print, __VA_ARGS__);

// qspi
void hal_lcd_qspi_panel_construct(mp_obj_base_t *self)
{
    mp_lcd_qspi_panel_obj_t *qspi_panel_obj = (mp_lcd_qspi_panel_obj_t *)self;
    machine_hw_spi_obj_t *spi_obj = ((machine_hw_spi_obj_t *)qspi_panel_obj->spi_obj);
    machine_hw_spi_obj_t old_spi_obj = *spi_obj;
    if (spi_obj->state == MACHINE_HW_SPI_STATE_INIT) {
        spi_obj->state = MACHINE_HW_SPI_STATE_DEINIT;
        machine_hw_spi_deinit_internal(&old_spi_obj);
    }

    mp_hal_pin_output(qspi_panel_obj->cs_pin);
    mp_hal_pin_od_high(qspi_panel_obj->cs_pin);

    spi_bus_config_t buscfg = {
        .data0_io_num = qspi_panel_obj->databus_pins[0],
        .data1_io_num = qspi_panel_obj->databus_pins[1],
        .sclk_io_num = spi_obj->sck,
        .data2_io_num = qspi_panel_obj->databus_pins[2],
        .data3_io_num = qspi_panel_obj->databus_pins[3],
        .max_transfer_sz = qspi_panel_obj->width * qspi_panel_obj->height * sizeof(uint16_t),
        //.max_transfer_sz = (0x4000 * 16) + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };
    esp_err_t ret = spi_bus_initialize(spi_obj->host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != 0) {
        mp_raise_msg_varg(&mp_type_OSError, "%d(spi_bus_initialize)", ret);
    }
    spi_obj->state = MACHINE_HW_SPI_STATE_INIT;

    spi_device_interface_config_t devcfg = {
        .command_bits = qspi_panel_obj->cmd_bits,
        .address_bits = 24,
        .mode = spi_obj->phase | (spi_obj->polarity << 1),
        .clock_speed_hz = qspi_panel_obj->pclk,
        .spics_io_num = -1,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 10,
    };

    ret = spi_bus_add_device(spi_obj->host, &devcfg, &qspi_panel_obj->io_handle);
    if (ret != 0) {
        mp_raise_msg_varg(&mp_type_OSError, "%d(spi_bus_add_device)", ret);
    }
}


inline void hal_lcd_qspi_panel_tx_param(mp_obj_base_t *self,
                                        int            lcd_cmd,
                                        const void    *param,
                                        size_t         param_size)
{
    DEBUG_printf("hal_lcd_qspi_panel_tx_param cmd: %x, param_size: %u\n", lcd_cmd, param_size);

    mp_lcd_qspi_panel_obj_t *qspi_panel_obj = (mp_lcd_qspi_panel_obj_t *)self;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = (SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR);
    t.cmd = 0x02;
    t.addr = lcd_cmd << 8;
    if (param_size != 0) {
        t.tx_buffer = param;
        t.length = qspi_panel_obj->cmd_bits * param_size;
    } else {
        t.tx_buffer = NULL;
        t.length = 0;
    }
    mp_hal_pin_od_low(qspi_panel_obj->cs_pin);
    spi_device_polling_transmit(qspi_panel_obj->io_handle, &t);
    mp_hal_pin_od_high(qspi_panel_obj->cs_pin);
}


inline void hal_lcd_qspi_panel_tx_color(mp_obj_base_t *self,
                                        int            lcd_cmd,
                                        const void    *color,
                                        size_t         color_size)
{
    DEBUG_printf("hal_lcd_qspi_panel_tx_color cmd: %x, color_size: %u\n", lcd_cmd, color_size);

    mp_lcd_qspi_panel_obj_t *qspi_panel_obj = (mp_lcd_qspi_panel_obj_t *)self;
    spi_transaction_ext_t t;

    mp_hal_pin_od_low(qspi_panel_obj->cs_pin);
    memset(&t, 0, sizeof(t));
    t.base.flags = SPI_TRANS_MODE_QIO;
    t.base.cmd = 0x32;
    t.base.addr = 0x002C00;
    spi_device_polling_transmit(qspi_panel_obj->io_handle, (spi_transaction_t *)&t);

    uint8_t *p_color = (uint8_t *)color;
    size_t chunk_size;
    size_t len = color_size;
    memset(&t, 0, sizeof(t));
    t.base.flags = SPI_TRANS_MODE_QIO | \
                       SPI_TRANS_VARIABLE_CMD | \
                       SPI_TRANS_VARIABLE_ADDR | \
                       SPI_TRANS_VARIABLE_DUMMY;
    t.command_bits = 0;
    t.address_bits = 0;
    t.dummy_bits = 0;
    do {
        if (len > 0x8000) {
            chunk_size = 0x8000;
        } else {
            chunk_size = len;
        }
        t.base.tx_buffer = p_color;
        t.base.length = chunk_size * 8;
        spi_device_polling_transmit(qspi_panel_obj->io_handle, (spi_transaction_t *)&t);
        len -= chunk_size;
        p_color += chunk_size;
    } while (len > 0);

    mp_hal_pin_od_high(qspi_panel_obj->cs_pin);
}


inline void hal_lcd_qspi_panel_deinit(mp_obj_base_t *self)
{
    // mp_lcd_qspi_panel_obj_t *qspi_panel_obj = (mp_lcd_qspi_panel_obj_t *)self;
    // esp_lcd_panel_io_del(qspi_panel_obj->io_handle);
}

