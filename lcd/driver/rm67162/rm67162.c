#include "rm67162.h"
#include "lcd_panel.h"
#include "qspi_panel.h"
#include "lcd_panel_commands.h"
#include "lcd_panel_types.h"
#include "rm67162_rotation.h"

#include "py/obj.h"
#include "py/runtime.h"
#include "mphalport.h"
#include "py/gc.h"

#include <string.h>

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

typedef struct _Point {
    mp_float_t x;
    mp_float_t y;
} Point;

typedef struct _Polygon {
    int length;
    Point *points;
} Polygon;

#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#define _swap_bytes(val) ((((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00))

#define ABS(N) (((N) < 0) ? (-(N)) : (N))
#define mp_hal_delay_ms(delay) (mp_hal_delay_us(delay * 1000))

int mod(int x, int m) {
    int r = x % m;
    return (r < 0) ? r + m : r;
}



STATIC void write_spi(mp_lcd_rm67162_obj_t *self, int cmd,const void *buf, int len) {
    if (self->lcd_panel_p) {
            self->lcd_panel_p->tx_param(self->bus_obj, cmd, buf, len);
    }
}

//esp_lcd_panel_io_tx_param() is used to write cmd through spi
STATIC void write_cmd(mp_lcd_rm67162_obj_t *self, int cmd, const void *data, int len) {
    if (data == NULL) {
        write_spi(self, cmd, NULL, 1);
    }
    if (data != NULL) {
        write_spi(self, cmd, data, len);
    }
}


STATIC void set_rotation(mp_lcd_rm67162_obj_t *self, uint8_t rotation)
{
    self->madctl_val &= 0x1F;
    self->madctl_val |= self->rotations[rotation].madctl;

    write_cmd(self, LCD_CMD_MADCTL, (uint8_t[]) { self->madctl_val }, 1);

    self->width = self->rotations[rotation].width;
    self->height = self->rotations[rotation].height;
    self->x_gap = self->rotations[rotation].colstart;
    self->y_gap = self->rotations[rotation].rowstart;
}


STATIC void mp_lcd_rm67162_print(const mp_print_t *print,
                                 mp_obj_t          self_in,
                                 mp_print_kind_t   kind)
{
    (void) kind;
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(
        print,
        "<RM67162 bus=%p, reset=%p, color_space=%s, bpp=%u>",
        self->bus_obj,
        self->reset,
        color_space_desc[self->color_space],
        self->bpp
    );
}


mp_obj_t mp_lcd_rm67162_make_new(const mp_obj_type_t *type,
                                 size_t               n_args,
                                 size_t               n_kw,
                                 const mp_obj_t      *all_args)
{
    enum {
        ARG_bus,
        ARG_reset,
        ARG_reset_level,
        ARG_color_space,
        ARG_bpp
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_bus,            MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = MP_OBJ_NULL}     },
        { MP_QSTR_reset,          MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = MP_OBJ_NULL}     },
        { MP_QSTR_reset_level,    MP_ARG_BOOL | MP_ARG_KW_ONLY, {.u_bool = false}          },
        { MP_QSTR_color_space,    MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = COLOR_SPACE_RGB} },
        { MP_QSTR_bpp,            MP_ARG_INT | MP_ARG_KW_ONLY,  {.u_int = 16}              },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(
        n_args,
        n_kw,
        all_args,
        MP_ARRAY_SIZE(allowed_args),
        allowed_args,
        args
    );

    // create new object
    mp_lcd_rm67162_obj_t *self = m_new_obj(mp_lcd_rm67162_obj_t);
    self->base.type = &mp_lcd_rm67162_type;

    self->bus_obj = (mp_obj_base_t *)MP_OBJ_TO_PTR(args[ARG_bus].u_obj);
#ifdef MP_OBJ_TYPE_GET_SLOT
    self->lcd_panel_p = (mp_lcd_panel_p_t *)MP_OBJ_TYPE_GET_SLOT(self->bus_obj->type, protocol);
#else
    self->lcd_panel_p = (mp_lcd_panel_p_t *)self->bus_obj->type->protocol;
#endif

    self->width = ((mp_lcd_qspi_panel_obj_t *)self->bus_obj)->width;
    self->height = ((mp_lcd_qspi_panel_obj_t *)self->bus_obj)->height;

    self->reset       = args[ARG_reset].u_obj;
    self->reset_level = args[ARG_reset_level].u_bool;
    self->color_space = args[ARG_color_space].u_int;
    self->bpp         = args[ARG_bpp].u_int;

    // reset
    if (self->reset != MP_OBJ_NULL) {
        mp_hal_pin_obj_t reset_pin = mp_hal_get_pin_obj(self->reset);
        mp_hal_pin_output(reset_pin);
    }

    switch (self->color_space) {
        case COLOR_SPACE_RGB:
            self->madctl_val = 0;
        break;

        case COLOR_SPACE_BGR:
            self->madctl_val |= (1 << 3);
        break;

        default:
            mp_raise_ValueError(MP_ERROR_TEXT("unsupported color space"));
        break;
    }

    switch (self->bpp) {
        case 16:
            self->colmod_cal = 0x75;
            self->fb_bpp = 16;
        break;

        case 18:
            self->colmod_cal = 0x76;
            self->fb_bpp = 24;
        break;

        case 24:
            self->colmod_cal = 0x77;
            self->fb_bpp = 24;
        break;

        default:
            mp_raise_ValueError(MP_ERROR_TEXT("unsupported pixel width"));
        break;
    }

    bzero(&self->rotations, sizeof(self->rotations));
    if ((self->width == 240 && self->height == 536) || \
        (self->width == 536 && self->height == 240)) {
        memcpy(&self->rotations, ORIENTATIONS_240x536, sizeof(ORIENTATIONS_240x536));
    } else {
        mp_warning(NULL, "rotation parameter not detected");
        mp_warning(NULL, "use default rotation parameters");
        memcpy(&self->rotations, ORIENTATIONS_GENERAL, sizeof(ORIENTATIONS_GENERAL));
        self->rotations[0].width = self->width;
        self->rotations[0].height = self->height;
        self->rotations[1].width = self->height;
        self->rotations[1].height = self->width;
        self->rotations[2].width = self->width;
        self->rotations[2].height = self->height;
        self->rotations[3].width = self->height;
        self->rotations[3].height = self->width;
    }
    set_rotation(self, 0);

    return MP_OBJ_FROM_PTR(self);
}


STATIC mp_obj_t mp_lcd_rm67162_deinit(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (self->lcd_panel_p) {
        self->lcd_panel_p->deinit(self->bus_obj);
    }

    // m_del_obj(mp_lcd_rm67162_obj_t, self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_deinit_obj, mp_lcd_rm67162_deinit);


STATIC mp_obj_t mp_lcd_rm67162_reset(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (self->reset != MP_OBJ_NULL) {
        mp_hal_pin_obj_t reset_pin = mp_hal_get_pin_obj(self->reset);
        mp_hal_pin_write(reset_pin, self->reset_level);
        mp_hal_delay_us(300 * 1000);
        mp_hal_pin_write(reset_pin, !self->reset_level);
        mp_hal_delay_us(200 * 1000);
    } else {
        write_cmd(self, LCD_CMD_SWRESET, NULL, 0);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_reset_obj, mp_lcd_rm67162_reset);


STATIC mp_obj_t mp_lcd_rm67162_init(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    write_cmd(self, LCD_CMD_SLPOUT, NULL, 0);     //sleep out
    mp_hal_delay_us(100 * 1000);

    write_cmd(self, LCD_CMD_MADCTL, (uint8_t[]) {
        self->madctl_val,
    }, 1);

    write_cmd(self, LCD_CMD_MADCTL, (uint8_t[]) {
        self->madctl_val,
    }, 1);

    write_cmd(self, LCD_CMD_COLMOD, (uint8_t[]) {
        self->colmod_cal,
    }, 1);

    // turn on display
    write_cmd(self, LCD_CMD_DISPON, NULL, 0);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_init_obj, mp_lcd_rm67162_init);


void param_list(uint8_t *len, const void* c_bits){
    uint8_t temp[len];
    for (int i = 0, i < len, i++) {
        temp[i] = (c_bits >> ((len - 1 - i) * 8)) & 0xff;
    }
    return temp;
}


STATIC mp_obj_t mp_lcd_rm67162_send_cmd(size_t n_args, const mp_obj_t *args_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(args_in[0]);
    uint8_t cmd = mp_obj_get_int(args_in[1]);
    mp_int_t c_bits = mp_obj_get_int(args_in[2]);
    uint8_t len = mp_obj_get_int(args_in[3]);

    write_cmd(self, cmd, param_list(len, c_bits), len);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lcd_rm67162_send_cmd_obj, 4, 4, mp_lcd_rm67162_send_cmd);


void draw_pixel(mp_lcd_rm67162_obj_t *self, int16_t x, int16_t y, uint16_t color) {
    if ((self->width < x) || (x < 0)) {
        x = mod(x, self->width);
    }
    if ((self->width < y) || (y < 0)) {
        y = mod(y, self->width);
    }

    write_cmd(self, LCD_CMD_CASET, (uint8_t[]) {
        ((x >> 8) & 0xFF),
        (x & 0xFF),
        (((x - 1) >> 8) & 0xFF),
        ((x - 1) & 0xFF),
    }, 4);
    write_cmd(self, LCD_CMD_RASET, (uint8_t[]) {
        ((y >> 8) & 0xFF),
        (y & 0xFF),
        (((y - 1) >> 8) & 0xFF),
        ((y - 1) & 0xFF),
    }, 4);
    self->lcd_panel_p->tx_color(
        self->bus_obj, 
        LCD_CMD_RAMWR, 
        (uint8_t[]) {
            (color >> 8) & 0xFF,
            color & 0xFF
        }, 
        2);
}


STATIC mp_obj_t mp_lcd_rm67162_pixel(size_t n_args, const mp_obj_t *args_in) {
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(args_in[0]);
    mp_int_t x = mp_obj_get_int(args_in[1]);
    mp_int_t y = mp_obj_get_int(args_in[2]);
    mp_int_t color = mp_obj_get_int(args_in[3]);

    draw_pixel(self, x, y, color);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lcd_rm67162_pixel_obj, 4, 4, mp_lcd_rm67162_pixel);


STATIC mp_obj_t mp_lcd_rm67162_bitmap(size_t n_args, const mp_obj_t *args_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(args_in[0]);

    int x_start = mp_obj_get_int(args_in[1]);
    int y_start = mp_obj_get_int(args_in[2]);
    int x_end   = mp_obj_get_int(args_in[3]);
    int y_end   = mp_obj_get_int(args_in[4]);

    x_start += self->x_gap;
    x_end += self->x_gap;
    y_start += self->y_gap;
    y_end += self->y_gap;

    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args_in[5], &bufinfo, MP_BUFFER_READ);
    write_cmd(self, LCD_CMD_CASET, (uint8_t[]) {
        ((x_start >> 8) & 0xFF),
        (x_start & 0xFF),
        (((x_end - 1) >> 8) & 0xFF),
        ((x_end - 1) & 0xFF),
    }, 4);
    write_cmd(self, LCD_CMD_RASET, (uint8_t[]) {
        ((y_start >> 8) & 0xFF),
        (y_start & 0xFF),
        (((y_end - 1) >> 8) & 0xFF),
        ((y_end - 1) & 0xFF),
    }, 4);
    size_t len = ((x_end - x_start) * (y_end - y_start) * self->fb_bpp / 8);
    self->lcd_panel_p->tx_color(self->bus_obj, LCD_CMD_RAMWR, bufinfo.buf, len);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lcd_rm67162_bitmap_obj, 6, 6, mp_lcd_rm67162_bitmap);


STATIC mp_obj_t mp_lcd_rm67162_mirror(mp_obj_t self_in,
                                      mp_obj_t mirror_x_in,
                                      mp_obj_t mirror_y_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (mp_obj_is_true(mirror_x_in)) {
        self->madctl_val |= (1 << 6);
    } else {
        self->madctl_val &= ~(1 << 6);
    }
    if (mp_obj_is_true(mirror_y_in)) {
        self->madctl_val |= (1 << 7);
    } else {
        self->madctl_val &= ~(1 << 7);
    }

    write_cmd(self, LCD_CMD_MADCTL, (uint8_t[]) {
            self->madctl_val
    }, 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mp_lcd_rm67162_mirror_obj, mp_lcd_rm67162_mirror);


STATIC mp_obj_t mp_lcd_rm67162_swap_xy(mp_obj_t self_in, mp_obj_t swap_axes_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (mp_obj_is_true(swap_axes_in)) {
        self->madctl_val |= 1 << 5;
    } else {
        self->madctl_val &= ~(1 << 5);
    }

    write_cmd(self, LCD_CMD_MADCTL, (uint8_t[]) {
            self->madctl_val
    }, 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_lcd_rm67162_swap_xy_obj, mp_lcd_rm67162_swap_xy);


STATIC mp_obj_t mp_lcd_rm67162_set_gap(mp_obj_t self_in,
                                       mp_obj_t x_gap_in,
                                       mp_obj_t y_gap_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    self->x_gap = mp_obj_get_int(x_gap_in);
    self->y_gap = mp_obj_get_int(y_gap_in);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mp_lcd_rm67162_set_gap_obj, mp_lcd_rm67162_set_gap);


STATIC mp_obj_t mp_lcd_rm67162_invert_color(mp_obj_t self_in, mp_obj_t invert_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (mp_obj_is_true(invert_in)) {
        write_cmd(self, LCD_CMD_INVON, NULL, 0);
    } else {
        write_cmd(self, LCD_CMD_INVOFF, NULL, 0);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_lcd_rm67162_invert_color_obj, mp_lcd_rm67162_invert_color);


STATIC mp_obj_t mp_lcd_rm67162_disp_off(mp_obj_t self_in, mp_obj_t off_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (mp_obj_is_true(off_in)) {
        write_cmd(self, LCD_CMD_DISPOFF, NULL, 0);
    } else {
        write_cmd(self, LCD_CMD_DISPON, NULL, 0);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_lcd_rm67162_disp_off_obj, mp_lcd_rm67162_disp_off);


STATIC mp_obj_t mp_lcd_rm67162_backlight_on(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    write_cmd(self, LCD_CMD_WRDISBV, (uint8_t[]) {
            0XFF
    }, 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_backlight_on_obj, mp_lcd_rm67162_backlight_on);


STATIC mp_obj_t mp_lcd_rm67162_backlight_off(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);

    write_cmd(self, LCD_CMD_WRDISBV, (uint8_t[]) {
            0x00
    }, 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_backlight_off_obj, mp_lcd_rm67162_backlight_off);


STATIC mp_obj_t mp_lcd_rm67162_brightness(mp_obj_t self_in, mp_obj_t brightness_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t brightness = mp_obj_get_int(brightness_in);

    if (brightness > 100) {
        brightness = 0xFF;
    } else if (brightness < 0) {
        brightness = 0x00;
    }

    write_cmd(self, LCD_CMD_WRDISBV, (uint8_t[]) {
            brightness & 0xFF
    }, 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_lcd_rm67162_brightness_obj, mp_lcd_rm67162_brightness);


STATIC mp_obj_t mp_lcd_rm67162_width(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->width);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_width_obj, mp_lcd_rm67162_width);


STATIC mp_obj_t mp_lcd_rm67162_height(mp_obj_t self_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_obj_new_int(self->height);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_lcd_rm67162_height_obj, mp_lcd_rm67162_height);


STATIC mp_obj_t mp_lcd_rm67162_rotation(size_t n_args, const mp_obj_t *args_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(args_in[0]);
    self->rotation = mp_obj_get_int(args_in[1]) % 4;
    if (n_args > 2) {
        mp_obj_tuple_t *rotations_in = MP_OBJ_TO_PTR(args_in[2]);
        for (size_t i = 0; i < rotations_in->len; i++) {
            if (i < 4) {
                mp_obj_tuple_t *item = MP_OBJ_TO_PTR(rotations_in->items[i]);
                self->rotations[i].madctl   = mp_obj_get_int(item->items[0]);
                self->rotations[i].width    = mp_obj_get_int(item->items[1]);
                self->rotations[i].height   = mp_obj_get_int(item->items[2]);
                self->rotations[i].colstart = mp_obj_get_int(item->items[3]);
                self->rotations[i].rowstart = mp_obj_get_int(item->items[4]);
            }
        }
    }
    set_rotation(self, self->rotation);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lcd_rm67162_rotation_obj, 2, 3, mp_lcd_rm67162_rotation);


STATIC mp_obj_t mp_lcd_rm67162_vscroll_area(size_t n_args, const mp_obj_t *args_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(args_in[0]);
    mp_int_t tfa = mp_obj_get_int(args_in[1]);
    mp_int_t vsa = mp_obj_get_int(args_in[2]);
    mp_int_t bfa = mp_obj_get_int(args_in[3]);

    write_cmd(
            self,
            LCD_CMD_VSCRDEF,
            (uint8_t []) {
                (tfa) >> 8,
                (tfa) & 0xFF,
                (vsa) >> 8,
                (vsa) & 0xFF,
                (bfa) >> 8,
                (bfa) & 0xFF
            },
            6
    );

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lcd_rm67162_vscroll_area_obj, 4, 4, mp_lcd_rm67162_vscroll_area);


STATIC mp_obj_t mp_lcd_rm67162_vscroll_start(size_t n_args, const mp_obj_t *args_in)
{
    mp_lcd_rm67162_obj_t *self = MP_OBJ_TO_PTR(args_in[0]);
    mp_int_t vssa = mp_obj_get_int(args_in[1]);

    if (n_args > 2) {
        if (mp_obj_is_true(args_in[2])) {
            self->madctl_val |= LCD_CMD_ML_BIT;
        } else {
            self->madctl_val &= ~LCD_CMD_ML_BIT;
        }
    } else {
        self->madctl_val &= ~LCD_CMD_ML_BIT;
    }
    write_cmd(
        self,
        LCD_CMD_MADCTL,
        (uint8_t[]) { self->madctl_val, },
        2
    );

    write_cmd(
        self,
        LCD_CMD_VSCSAD,
        (uint8_t []) { (vssa) >> 8, (vssa) & 0xFF },
        2
    );

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_lcd_rm67162_vscroll_start_obj, 2, 3, mp_lcd_rm67162_vscroll_start);


STATIC const mp_rom_map_elem_t mp_lcd_rm67162_locals_dict_table[] = {
    /* { MP_ROM_QSTR(MP_QSTR_custom_init),   MP_ROM_PTR(&mp_lcd_rm67162_custom_init_obj)   }, */
    { MP_ROM_QSTR(MP_QSTR_deinit),        MP_ROM_PTR(&mp_lcd_rm67162_deinit_obj)        },
    { MP_ROM_QSTR(MP_QSTR_reset),         MP_ROM_PTR(&mp_lcd_rm67162_reset_obj)         },
    { MP_ROM_QSTR(MP_QSTR_init),          MP_ROM_PTR(&mp_lcd_rm67162_init_obj)          },
    { MP_ROM_QSTR(MP_QSTR_send_cmd),      MP_ROM_PTR(&mp_lcd_rm67162_send_cmd_obj)      },
    { MP_ROM_QSTR(MP_QSTR_pixel)          MP_ROM_PTR(&mp_lcd_rm67162_pixel_obj)         },
    { MP_ROM_QSTR(MP_QSTR_bitmap),        MP_ROM_PTR(&mp_lcd_rm67162_bitmap_obj)        },
    { MP_ROM_QSTR(MP_QSTR_mirror),        MP_ROM_PTR(&mp_lcd_rm67162_mirror_obj)        },
    { MP_ROM_QSTR(MP_QSTR_swap_xy),       MP_ROM_PTR(&mp_lcd_rm67162_swap_xy_obj)       },
    { MP_ROM_QSTR(MP_QSTR_set_gap),       MP_ROM_PTR(&mp_lcd_rm67162_set_gap_obj)       },
    { MP_ROM_QSTR(MP_QSTR_invert_color),  MP_ROM_PTR(&mp_lcd_rm67162_invert_color_obj)  },
    { MP_ROM_QSTR(MP_QSTR_disp_off),      MP_ROM_PTR(&mp_lcd_rm67162_disp_off_obj)      },
    { MP_ROM_QSTR(MP_QSTR_backlight_on),  MP_ROM_PTR(&mp_lcd_rm67162_backlight_on_obj)  },
    { MP_ROM_QSTR(MP_QSTR_backlight_off), MP_ROM_PTR(&mp_lcd_rm67162_backlight_off_obj) },
    { MP_ROM_QSTR(MP_QSTR_brightness),    MP_ROM_PTR(&mp_lcd_rm67162_brightness_obj)    },
    { MP_ROM_QSTR(MP_QSTR_height),        MP_ROM_PTR(&mp_lcd_rm67162_height_obj)        },
    { MP_ROM_QSTR(MP_QSTR_width),         MP_ROM_PTR(&mp_lcd_rm67162_width_obj)         },
    { MP_ROM_QSTR(MP_QSTR_rotation),      MP_ROM_PTR(&mp_lcd_rm67162_rotation_obj)      },
    { MP_ROM_QSTR(MP_QSTR_vscroll_area),  MP_ROM_PTR(&mp_lcd_rm67162_vscroll_area_obj)  },
    { MP_ROM_QSTR(MP_QSTR_vscroll_start), MP_ROM_PTR(&mp_lcd_rm67162_vscroll_start_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__),       MP_ROM_PTR(&mp_lcd_rm67162_deinit_obj)        },

    { MP_ROM_QSTR(MP_QSTR_RGB),           MP_ROM_INT(COLOR_SPACE_RGB)                   },
    { MP_ROM_QSTR(MP_QSTR_BGR),           MP_ROM_INT(COLOR_SPACE_BGR)                   },
    { MP_ROM_QSTR(MP_QSTR_MONOCHROME),    MP_ROM_INT(COLOR_SPACE_MONOCHROME)            },
};
STATIC MP_DEFINE_CONST_DICT(mp_lcd_rm67162_locals_dict, mp_lcd_rm67162_locals_dict_table);


#ifdef MP_OBJ_TYPE_GET_SLOT
MP_DEFINE_CONST_OBJ_TYPE(
    mp_lcd_rm67162_type,
    MP_QSTR_RM67162,
    MP_TYPE_FLAG_NONE,
    print, mp_lcd_rm67162_print,
    make_new, mp_lcd_rm67162_make_new,
    locals_dict, (mp_obj_dict_t *)&mp_lcd_rm67162_locals_dict
);
#else
const mp_obj_type_t mp_lcd_rm67162_type = {
    { &mp_type_type },
    .name        = MP_QSTR_RM67162,
    .print       = mp_lcd_rm67162_print,
    .make_new    = mp_lcd_rm67162_make_new,
    .locals_dict = (mp_obj_dict_t *)&mp_lcd_rm67162_locals_dict,
};
#endif
