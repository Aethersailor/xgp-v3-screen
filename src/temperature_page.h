// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef XGP_TEMPERATURE_PAGE_H
#define XGP_TEMPERATURE_PAGE_H

#include "lvgl/lvgl.h"

extern lv_obj_t *ui_Temperature;
extern lv_obj_t *ui_txtTemperatureNumber;

void temperature_page_init(void);
void temperature_page_destroy(void);
void temperature_page_set_modem(const char *reading);

#endif
