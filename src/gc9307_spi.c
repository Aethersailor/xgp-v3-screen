// SPDX-License-Identifier: GPL-3.0-or-later
// GC9307 register settings adapted from zzzz0317/kmod-fb-tft-gc9307 and the XiGuaPi V3 Lua driver.

#include "gc9307_spi.h"
#include "lvgl/lvgl.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define PANEL_WIDTH 320
#define PANEL_HEIGHT 240
#define DC_LINE (1U << 0)
#define RESET_LINE (1U << 1)
#define BACKLIGHT_PATH "/sys/class/leds/blue:backlight/brightness"

static int spi_fd = -1;
static int lines_fd = -1;
static bool backlight_on;
static uint32_t draw_buffer[PANEL_WIDTH * 24 * sizeof(uint16_t) / sizeof(uint32_t)];
// The board's spidev buffer is 4096 bytes. Keep each transfer on a pixel boundary.
static uint8_t wire_buffer[3840];

static void fail(const char *operation)
{
    perror(operation);
    exit(EXIT_FAILURE);
}

static int set_backlight(bool on)
{
    const char *value = on ? "255\n" : "0\n";
    const size_t size = strlen(value);
    int fd = open(BACKLIGHT_PATH, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    ssize_t written = write(fd, value, size);
    int error = written < 0 ? errno : EIO;
    close(fd);
    if (written != (ssize_t)size) {
        errno = error;
        return -1;
    }
    return 0;
}

static void close_panel(void)
{
    if (backlight_on && set_backlight(false) < 0)
        perror("turn off GC9307 backlight");
    if (spi_fd >= 0)
        close(spi_fd);
    close(lines_fd);
}

static void open_gpio(void)
{
    struct gpio_v2_line_request request = {
        .offsets = { 21, 20 },
        .consumer = "xgp-v3-screen",
        .config = {
            .flags = GPIO_V2_LINE_FLAG_OUTPUT,
            .num_attrs = 1,
            .attrs = {{
                .attr = {
                    .id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES,
                    .values = RESET_LINE,
                },
                .mask = DC_LINE | RESET_LINE,
            }},
        },
        .num_lines = 2,
    };

    int chip_fd = open("/dev/gpiochip4", O_RDONLY | O_CLOEXEC);
    if (chip_fd < 0)
        fail("open /dev/gpiochip4");
    if (ioctl(chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0)
        fail("request GC9307 D/C and RESET GPIOs");
    lines_fd = request.fd;
    close(chip_fd);
}

static void gpio_set(uint64_t mask, bool high)
{
    struct gpio_v2_line_values values = {
        .bits = high ? mask : 0,
        .mask = mask,
    };
    if (ioctl(lines_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0)
        fail("set GC9307 GPIO value");
}

static void spi_write(const void *data, size_t size)
{
    ssize_t written = write(spi_fd, data, size);
    if (written != (ssize_t)size) {
        if (written >= 0)
            errno = EIO;
        fail("write GC9307 SPI data");
    }
}

static void command(uint8_t opcode, const uint8_t *params, size_t size)
{
    gpio_set(DC_LINE, false);
    spi_write(&opcode, sizeof(opcode));
    if (size) {
        gpio_set(DC_LINE, true);
        spi_write(params, size);
    }
}

static void configure_panel(void)
{
    static const struct {
        uint8_t opcode;
        uint8_t size;
        uint8_t params[14];
    } settings[] = {
        { 0x3A, 1, { 0x05 } },
        { 0xB2, 5, { 0x08, 0x08, 0x00, 0x22, 0x22 } },
        { 0xB7, 1, { 0x35 } },
        { 0xC2, 2, { 0x01, 0xFF } },
        { 0xC3, 1, { 0x0B } },
        { 0xC4, 1, { 0x20 } },
        { 0xBB, 1, { 0x20 } },
        { 0xC5, 1, { 0x20 } },
        { 0xD0, 2, { 0xA4, 0xA1 } },
        { 0x36, 1, { 0x38 } }, // XiGuaPi V3 landscape address order.
        { 0xE0, 14, { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05, 0x26,
                      0x44, 0x45, 0x0F, 0x17, 0x16, 0x2B, 0x33 } },
        { 0xE1, 14, { 0xD0, 0x05, 0x0A, 0x09, 0x08, 0x05, 0x26,
                      0x43, 0x45, 0x0F, 0x16, 0x16, 0x2B, 0x33 } },
    };

    gpio_set(RESET_LINE, false);
    lv_delay_ms(20);
    gpio_set(RESET_LINE, true);
    lv_delay_ms(120);
    command(0x11, NULL, 0);
    lv_delay_ms(120);

    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i)
        command(settings[i].opcode, settings[i].params, settings[i].size);

    command(0x29, NULL, 0);
    lv_delay_ms(20);
}

static void set_window(uint8_t opcode, uint16_t first, uint16_t last)
{
    const uint8_t params[] = { first >> 8, first & 0xff, last >> 8, last & 0xff };
    command(opcode, params, sizeof(params));
}

static void flush_pixels(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    const size_t width = lv_area_get_width(area);
    const size_t height = lv_area_get_height(area);
    const size_t stride = lv_display_get_buf_active(display)->header.stride;
    size_t used = 0;

    set_window(0x2A, area->x1, area->x2);
    set_window(0x2B, area->y1, area->y2);
    command(0x2C, NULL, 0);
    gpio_set(DC_LINE, true);

    for (size_t y = 0; y < height; ++y) {
        const uint8_t *row = pixels + y * stride;
        for (size_t x = 0; x < width; ++x) {
            uint16_t pixel;
            memcpy(&pixel, row + x * sizeof(pixel), sizeof(pixel));
            // LVGL draws native RGB565. The SPI wire carries the high byte first.
            wire_buffer[used++] = pixel >> 8;
            wire_buffer[used++] = pixel & 0xff;
            if (used == sizeof(wire_buffer)) {
                spi_write(wire_buffer, used);
                used = 0;
            }
        }
    }
    if (used)
        spi_write(wire_buffer, used);

    if (!backlight_on && lv_display_flush_is_last(display)) {
        if (set_backlight(true) < 0)
            fail("turn on GC9307 backlight");
        backlight_on = true;
    }
    lv_display_flush_ready(display);
}

void gc9307_spi_init(void)
{
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 8000000;

    // Acquire both GPIOs before touching the panel or backlight.
    open_gpio();
    if (atexit(close_panel) != 0) {
        fputs("cannot register GC9307 cleanup\n", stderr);
        exit(EXIT_FAILURE);
    }

    spi_fd = open("/dev/spidev3.0", O_WRONLY | O_CLOEXEC);
    if (spi_fd < 0)
        fail("open /dev/spidev3.0");
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
        fail("configure GC9307 SPI");

    if (set_backlight(false) < 0)
        fail("turn off GC9307 backlight");
    configure_panel();

    lv_display_t *display = lv_display_create(PANEL_WIDTH, PANEL_HEIGHT);
    if (display == NULL) {
        fputs("cannot allocate GC9307 display\n", stderr);
        exit(EXIT_FAILURE);
    }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_pixels);
}
