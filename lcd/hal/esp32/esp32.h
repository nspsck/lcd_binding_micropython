#ifndef _ESP32_H_
#define _ESP32_H_

#include "py/obj.h"

// qspi
void hal_lcd_qspi_panel_construct(mp_obj_base_t *self);

void hal_lcd_qspi_panel_tx_param(
    mp_obj_base_t *self,
    int lcd_cmd,
    const void *param,
    size_t param_size
);

void hal_lcd_qspi_panel_tx_color(
    mp_obj_base_t *self,
    int lcd_cmd,
    const void *color,
    size_t color_size
);

void hal_lcd_qspi_panel_deinit(mp_obj_base_t *self);

void hal_lcd_dpi_mirror(mp_obj_base_t *self, bool mirror_x, bool mirror_y);

void hal_lcd_dpi_swap_xy(mp_obj_base_t *self, bool swap_axes);

void hal_lcd_dpi_set_gap(mp_obj_base_t *self, int x_gap, int y_gap);

void hal_lcd_dpi_invert_color(mp_obj_base_t *self, bool invert_color_data);

void hal_lcd_dpi_disp_off(mp_obj_base_t *self, bool off);

#endif