#ifndef _MAIN_H
#define _MAIN_H

#include "lvgl/lvgl.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void lvgl_setup();
void lvgl_loop();






void lv_rt_loga(const char *buf);
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif

#endif