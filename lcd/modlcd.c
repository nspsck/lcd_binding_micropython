#include "rm67162.h"
#include "qspi_panel.h"
#include "lcd_panel_types.h"

#include "py/obj.h"

STATIC const mp_map_elem_t mp_module_lcd_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),   MP_OBJ_NEW_QSTR(MP_QSTR_lcd)          },
    { MP_ROM_QSTR(MP_QSTR_RM67162),    (mp_obj_t)&mp_lcd_rm67162_type        },
    { MP_ROM_QSTR(MP_QSTR_QSPIPanel),  (mp_obj_t)&mp_lcd_qspi_panel_type     },
    { MP_ROM_QSTR(MP_QSTR_RGB),        MP_ROM_INT(COLOR_SPACE_RGB)           },
    { MP_ROM_QSTR(MP_QSTR_BGR),        MP_ROM_INT(COLOR_SPACE_BGR)           },
    { MP_ROM_QSTR(MP_QSTR_MONOCHROME), MP_ROM_INT(COLOR_SPACE_MONOCHROME)    },
};
STATIC MP_DEFINE_CONST_DICT(mp_module_lcd_globals, mp_module_lcd_globals_table);


const mp_obj_module_t mp_module_lcd = {
    .base    = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&mp_module_lcd_globals,
};


#if MICROPY_VERSION >= 0x011300 // MicroPython 1.19 or later
MP_REGISTER_MODULE(MP_QSTR_lcd, mp_module_lcd);
#else
MP_REGISTER_MODULE(MP_QSTR_lcd, mp_module_lcd, MODULE_LCD_ENABLE);
#endif
