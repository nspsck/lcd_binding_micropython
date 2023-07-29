LCD Driver for MicroPython
------------------------------

Contents:

- [LCD Driver for MicroPython](#lcd-driver-for-micropython)
- [Introduction](#introduction)
- [Features](#features)
- [Documentation](#documentation)

## Introduction
This is a fork from Ibuque's lcd_binding_micropython specific for the RM67162 (Used in T-AMOLED S3). Original repo: [lcd_binding_micropython](https://github.com/lbuque/lcd_binding_micropython). 

This driver is based on [esp_lcd](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd.html).

The performance has been increased by roughly 700% (only applies to the RM67162 driver). Now supports the newest Micropython using esp-idf v5.0.2.

- changed the program structure for more readability
- added brightness control
- fixed the initialization bug using tft_config.py
- Drawing functions: fill, fill_rect, rect, fill_cirlce, cirlce, pixel, line, vline, hline, colorRGB

To-DO:
- Fontsupport: bitmap fonts
- png support

## Features

The following display driver ICs are supported:
- Support for RM67162 displays

Supported boards：
- [LILYGO T-DisplayS3-AMOLED](https://github.com/Xinyuan-LilyGO/T-Display-S3-AMOLED)

| Driver IC | Hardware SPI     | Software SPI     | Hardware QSPI    | I8080            | DPI(RGB)         |
| --------- | ---------------- | ---------------- | ---------------- | ---------------- | ---------------- |
| ESP32-S3  | ![alt text][2]   | ![alt text][2]   | ![alt text][1]   | ![alt text][2]   | ![alt text][2]   |

[1]: https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e
[2]: https://img.shields.io/badge/-not%20support-lightgrey
[3]: https://img.shields.io/badge/-untested-red
[4]: https://img.shields.io/badge/-todo-blue

## Documentation
In general, the screen starts at 0, and goes to 535 x 239, that's a total resolution of 536 x 240. All drawing functions should be called with this in mind.

- `init()`

  Must be called to initialize the display.

- `deinit()`

  Deinit the tft object and release the memory used for the framebuffer.

- `reset()`

  Soft reset the display.

- `rotation(value)`

  Rotate the display, value range: 0 - 3.

- `brightness(value)`

  Set the screen brightness, value range: 0 - 100, in percentage.

- `disp_off()`

  Turn off the display.

- `disp_on()`

  Turn on the display.

- `backlight_on()`

  Turn on backlight, this is equal to `brightness(100)`.

- `backlight_off()`

  Turn off backlight, this is equal to `brightness(0)`.

- `invert_color()`

  Invert the display color.

- `height()`

  Returns the height of the display.

- `width()`

  Returns the width of the display.

- `colorRGB(r, g, b)`

  Call this function to get the rgb color for the drawing.

- `pixel(x, y, color)`

  Draw a single pixel at the postion (x, y) with color.

- `hline(x, y, l, color)`

  Draw a horizontal line starting at the postion (x, y) with color and length l. 

- `vline(x, y, l, color)`

  Draw a vertical line starting at the postion (x, y) with color and length l.

- `fill(color)`

  Fill the entire screen with the color.

- `fill_rect(x, y, w, h, color)`

  Draw a rectangle starting from (x, y) with the width w and height h and fill it with the color.

- `rect(x, y, w, h, color)`

  Draw a rectangle starting from (x, y) with the width w and height h of the color.

- `fill_circle(x, y, r, color)`

  Draw a circle with the middle point (x, y) with the radius r and fill it with the color.

- `circle(x, y, r, color)`

  Draw a circle with the middle point (x, y) with the radius r of the color.

- `bitmap(x0, y0, x1, y1, buf)`

  Bitmap the content of a bytearray buf filled with color565 values starting from (x0, y0) to (x1, y1). Currently, the user is resposible for the provided buf content.


## Related Repositories

- [framebuf-plus](https://github.com/lbuque/framebuf-plus)
