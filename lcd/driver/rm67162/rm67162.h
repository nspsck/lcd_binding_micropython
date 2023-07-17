#ifndef LCD_RM67162_H_
#define LCD_RM67162_H_

#include "py/obj.h"

extern const mp_obj_type_t mp_lcd_rm67162_type;


// this is the actual C-structure for our new object
typedef struct _mp_lcd_rm67162_obj_t {
    mp_obj_base_t base;
    mp_obj_base_t *bus_obj;
    mp_lcd_panel_p_t *lcd_panel_p;
    mp_obj_t reset;
    bool reset_level;
    uint8_t color_space;

    uint16_t width;
    uint16_t height;
    uint8_t rotation;
    lcd_panel_rotation_t rotations[4];   // list of rotation tuples
    int x_gap;
    int y_gap;
    uint32_t bpp;
    uint8_t fb_bpp;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_cal; // save surrent value of LCD_CMD_COLMOD register
} mp_lcd_rm67162_obj_t;


#endif