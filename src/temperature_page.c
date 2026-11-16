// SPDX-License-Identifier: GPL-3.0-or-later
#include "temperature_page.h"
#include "ui/ui.h"

#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum temperature_field {
    TEMPERATURE_CPU,
    TEMPERATURE_GPU,
    TEMPERATURE_WIFI,
    TEMPERATURE_MODEM,
    TEMPERATURE_COUNT
};

lv_obj_t *ui_Temperature;
lv_obj_t *ui_txtTemperatureNumber;

static lv_obj_t *values[TEMPERATURE_COUNT];
static lv_timer_t *sample_timer;
static double modem_temperature = NAN;

static double read_sensor(const char *directory, const char *name_file,
                          const char *pattern, const char *input_file)
{
    DIR *devices = opendir(directory);
    if (!devices)
        return NAN;

    char path[PATH_MAX];
    double temperature = NAN;
    struct dirent *device;
    while ((device = readdir(devices)) != NULL) {
        if (device->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s/%s", directory, device->d_name, name_file);
        FILE *file = fopen(path, "r");
        if (!file)
            continue;

        char name[64];
        const bool have_name = fgets(name, sizeof(name), file) != NULL;
        fclose(file);
        if (!have_name)
            continue;
        name[strcspn(name, "\n")] = '\0';
        if (fnmatch(pattern, name, 0) != 0)
            continue;

        snprintf(path, sizeof(path), "%s/%s/%s", directory, device->d_name, input_file);
        file = fopen(path, "r");
        if (!file)
            continue;

        long millidegrees;
        const bool have_temperature = fscanf(file, "%ld", &millidegrees) == 1;
        fclose(file);
        if (have_temperature) {
            temperature = millidegrees / 1000.0;
            break;
        }
    }

    closedir(devices);
    return temperature;
}

static void show_temperature(enum temperature_field field, double temperature)
{
    char text[24] = "--";
    if (isfinite(temperature))
        snprintf(text, sizeof(text), field == TEMPERATURE_WIFI ? "%.0f" : "%.1f", temperature);
    lv_label_set_text(values[field], text);
}

static void sample_board_temperatures(lv_timer_t *timer)
{
    (void)timer;
    show_temperature(TEMPERATURE_CPU, read_sensor("/sys/class/thermal", "type", "cpu-thermal", "temp"));
    show_temperature(TEMPERATURE_GPU, read_sensor("/sys/class/thermal", "type", "gpu-thermal", "temp"));
    show_temperature(TEMPERATURE_WIFI, read_sensor("/sys/class/hwmon", "name", "mt7925_*", "temp1_input"));
}

static void handle_page_transition(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        sample_board_temperatures(NULL);
        show_temperature(TEMPERATURE_MODEM, modem_temperature);
        lv_timer_reset(sample_timer);
        lv_timer_resume(sample_timer);
    } else {
        lv_timer_pause(sample_timer);
    }
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void create_card(enum temperature_field field, const char *name, int x, int y)
{
    lv_obj_t *card = lv_obj_create(ui_Temperature);
    lv_obj_remove_style_all(card);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, 138, 62);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x22272f), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x3b4654), 0);
    lv_obj_set_style_border_width(card, 1, 0);

    lv_obj_t *caption = create_label(card, name, &ui_font_MiSans16, 0xbecad8);
    lv_obj_align(caption, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *reading = lv_obj_create(card);
    lv_obj_remove_style_all(reading);
    lv_obj_remove_flag(reading, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(reading, lv_pct(100), 29);
    lv_obj_align(reading, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(reading, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reading, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(reading, 4, 0);
    values[field] = create_label(reading, "--", &ui_font_MiSans24, 0xf5f7fa);
    create_label(reading, "°C", &ui_font_MiSans16, 0xbecad8);
}

void temperature_page_init(void)
{
    ui_Temperature = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_Temperature, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = create_label(ui_Temperature, "温度监控", &ui_font_MiSans16, 0xffffff);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);
    ui_txtTemperatureNumber = create_label(ui_Temperature, "", &ui_font_MiSans16, 0xffffff);
    lv_obj_align(ui_txtTemperatureNumber, LV_ALIGN_TOP_RIGHT, -14, 42);

    create_card(TEMPERATURE_CPU, "CPU", 14, 71);
    create_card(TEMPERATURE_GPU, "GPU", 168, 71);
    create_card(TEMPERATURE_WIFI, "Wi-Fi", 14, 141);
    create_card(TEMPERATURE_MODEM, "5G 模组", 168, 141);

    sample_timer = lv_timer_create(sample_board_temperatures, 3000, NULL);
    lv_timer_pause(sample_timer);
    lv_obj_add_event_cb(ui_Temperature, handle_page_transition, LV_EVENT_SCREEN_LOAD_START, NULL);
    lv_obj_add_event_cb(ui_Temperature, handle_page_transition, LV_EVENT_SCREEN_UNLOAD_START, NULL);
}

void temperature_page_destroy(void)
{
    if (ui_Temperature) {
        lv_obj_delete(ui_Temperature);
        lv_timer_delete(sample_timer);
        ui_Temperature = NULL;
        ui_txtTemperatureNumber = NULL;
    }
}

void temperature_page_set_modem(const char *reading)
{
    char *unit;
    const double value = strtod(reading, &unit);
    const bool have_value = unit != reading && isfinite(value);
    while (isspace((unsigned char)*unit))
        unit++;
    modem_temperature = have_value && (!*unit || strcmp(unit, "°C") == 0 || strcmp(unit, "℃") == 0)
        ? value : NAN;
    if (ui_Temperature && lv_screen_active() == ui_Temperature)
        show_temperature(TEMPERATURE_MODEM, modem_temperature);
}
