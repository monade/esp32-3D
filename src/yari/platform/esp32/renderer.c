#include <stdint.h>
#include <string.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "renderer.h"
#include "inputs.h"
#include "colors.h"

// Panel size (T-Display active area)
#ifndef LCD_W
#define LCD_W 240
#endif
#ifndef LCD_H
#define LCD_H 136
#endif
#ifndef LCD_X_OFF
#define LCD_X_OFF 40
#endif
#ifndef LCD_Y_OFF
#define LCD_Y_OFF 53
#endif

// T-Display (ST7789) std pins
#ifndef PIN_MOSI
#define PIN_MOSI 19
#endif
#ifndef PIN_CLK
#define PIN_CLK 18
#endif
#ifndef PIN_CS
#define PIN_CS 5
#endif
#ifndef PIN_DC
#define PIN_DC 16
#endif
#ifndef PIN_RST
#define PIN_RST 23
#endif
#ifndef PIN_BL
#define PIN_BL 4
#endif
#ifndef SPI_CLOCK_SPEED
#define SPI_CLOCK_SPEED (80 * 1000 * 1000)
#endif

#define SCREEN_BUFFER_SIZE (LCD_W * LCD_H * sizeof(uint16_t))
#define SCREEN_PIXEL_COUNT (LCD_W * LCD_H)

static int64_t last_frame_start_us = 0;
static int64_t frame_start_time_us = 0;
static float cached_frame_time = 0.0f;
static int target_fps = 30;
static int64_t target_frame_time_us = 1000000 / 30;
static bool lcd_frame_window_set = false;

static uint16_t framebuffer0[SCREEN_PIXEL_COUNT];

static uint16_t *fb_back = framebuffer0;
static spi_transaction_t fb_trans = {
    .length = SCREEN_BUFFER_SIZE * 8,
};

// ST7789 wants big-endian RGB565 on the wire, so the framebuffer stores pixels
// byte-swapped and is streamed out verbatim. This is the single swap helper used
// by every write path in this file.
static inline uint16_t lcd_color(yr_pixel_t color) {
    uint16_t c = (uint16_t)color;
    return (uint16_t)((c << 8) | (c >> 8));
}

static inline void fill_pixels(uint16_t *dst, int count, uint16_t color) {
    if (count <= 0) return;

    if (((uintptr_t)dst & 2) != 0) {
        *dst++ = color;
        count--;
    }

    uint32_t color2 = (uint32_t)color | ((uint32_t)color << 16);
    uint32_t *dst32 = (uint32_t *)dst;

    while (count >= 2) {
        *dst32++ = color2;
        count -= 2;
    }

    if (count > 0) {
        *(uint16_t *)dst32 = color;
    }
}

static spi_device_handle_t spi;

static void lcd_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = { cmd },
    };
    spi_device_polling_transmit(spi, &t);
}

static void lcd_data(const void *data, int len_bytes) {
    if (len_bytes <= 0) return;
    gpio_set_level(PIN_DC, 1);
    spi_transaction_t t = {0};
    t.length = len_bytes * 8;
    t.tx_buffer = data;
    spi_device_polling_transmit(spi, &t);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    x0 += LCD_X_OFF;
    x1 += LCD_X_OFF;
    y0 += LCD_Y_OFF;
    y1 += LCD_Y_OFF;

    uint8_t buf[4];

    lcd_cmd(0x2A); // CASET
    buf[0] = x0 >> 8;
    buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8;
    buf[3] = x1 & 0xFF;
    lcd_data(buf, 4);

    lcd_cmd(0x2B); // RASET
    buf[0] = y0 >> 8;
    buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8;
    buf[3] = y1 & 0xFF;
    lcd_data(buf, 4);

    lcd_cmd(0x2C); // RAMWR
}


static void lcd_init(void) {
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);

    // Reset
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // SPI bus
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SCREEN_BUFFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_CLOCK_SPEED,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_HALFDUPLEX
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &spi));

    // ST7789 init sequence
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(150)); // SWRESET
    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120)); // SLPOUT

    uint8_t colmod = 0x55; // 16-bit
    lcd_cmd(0x3A);
    lcd_data(&colmod, 1);

    // Rotation landscape
    uint8_t madctl = 0x60;
    lcd_cmd(0x36);
    lcd_data(&madctl, 1);

    lcd_cmd(0x21); // INVON
    lcd_cmd(0x13); // NORON
    lcd_cmd(0x29); // DISPON

    gpio_set_level(PIN_BL, 1);
}

void IRAM_ATTR yr_draw_rectangle(int posX, int posY, int width, int height, yr_pixel_t color) {
    if (width == 1 && height == 1) {
        if ((unsigned)posX < LCD_W && (unsigned)posY < LCD_H) {
            fb_back[posY * LCD_W + posX] = lcd_color(color);
        }
        return;
    }

    if (width <= 0 || height <= 0) return;

    // Clipping
    if (posX < 0) {
        width += posX;
        posX = 0;
    }
    if (posY < 0) {
        height += posY;
        posY = 0;
    }
    if (posX + width > LCD_W) width = LCD_W - posX;
    if (posY + height > LCD_H) height = LCD_H - posY;
    if (width <= 0 || height <= 0) return;

    uint16_t native_color = lcd_color(color);
    uint16_t *dst = fb_back + posY * LCD_W + posX;

    if (width == 1) {
        for (int y = 0; y < height; y++) {
            *dst = native_color;
            dst += LCD_W;
        }
        return;
    }

    for (int y = 0; y < height; y++) {
        fill_pixels(dst, width, native_color);
        dst += LCD_W;
    }
}

void IRAM_ATTR yr_clear_screen(yr_pixel_t color) {
    fill_pixels(fb_back, SCREEN_PIXEL_COUNT, lcd_color(color));
}

typedef struct {
    YrColorFilterCallback apply;
    void *user_data;
} yr_filter_job_ctx;

// Applies the filter to framebuffer rows [y_start, y_end). With
// ESP32_MULTITHREAD this runs concurrently on both cores over disjoint row
// ranges, so the callback must be safe to call from either core.
static void IRAM_ATTR yr_filter_rows(void *arg, int y_start, int y_end) {
    const yr_filter_job_ctx *ctx = (const yr_filter_job_ctx *)arg;

    uint16_t *px = fb_back + y_start * LCD_W;
    for (int y = y_start; y < y_end; y++) {
        for (int x = 0; x < LCD_W; x++, px++) {
            // The framebuffer holds panel-order (byte-swapped) pixels; the swap
            // is its own inverse, so lcd_color converts in both directions.
            yr_pixel_t color = (yr_pixel_t)lcd_color(*px);
            ctx->apply(x, y, &color, ctx->user_data);
            *px = lcd_color(color);
        }
    }
}

void IRAM_ATTR yr_apply_color_filter(YrColorFilterCallback apply, void *user_data) {
    if (!apply) return;

    yr_filter_job_ctx ctx = { .apply = apply, .user_data = user_data };
#ifdef ESP32_MULTITHREAD
    yr_run_split(yr_filter_rows, &ctx, LCD_H);
#else
    yr_filter_rows(&ctx, 0, LCD_H);
#endif
}

yr_pixel_t *get_framebuffer() {
    return (yr_pixel_t *)fb_back;
}


static void set_target_fps(unsigned int fps) {
    target_fps = fps;
    target_frame_time_us = fps > 0 ? 1000000 / fps : 0;
}

void yr_renderer_init(int width, int height, const char *title, unsigned int target_fps) {
    (void)width;
    (void)height;
    (void)title;
    memset(framebuffer0, 0, sizeof(framebuffer0));
    lcd_init();
    yr_inputs_init();
    set_target_fps(target_fps);
    lcd_frame_window_set = false;
}

bool yr_game_should_close() {
    return 0;
}

void yr_begin_drawing() {
    int64_t now = esp_timer_get_time();
    cached_frame_time = (last_frame_start_us == 0)
                            ? 0.0f
                            : (float)(now - last_frame_start_us) / 1000000.0f;
    last_frame_start_us = now;
    frame_start_time_us = now;
}

void yr_render_screen() {
  if (!lcd_frame_window_set) {
    lcd_set_window(0, 0, LCD_W - 1, LCD_H - 1);
    lcd_frame_window_set = true;
  } else {
    lcd_cmd(0x2C);
  }
  gpio_set_level(PIN_DC, 1);

  fb_trans.tx_buffer = fb_back;
  ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &fb_trans));
}

void yr_end_drawing() {
    if (target_fps > 0) {
        int64_t target_end_us = frame_start_time_us + target_frame_time_us;
        int64_t sleep_time_us = target_end_us - esp_timer_get_time();

        if (sleep_time_us > 0) {
            // Block-sleep the bulk (yields the CPU so the idle task feeds the
            // watchdog), then spin the sub-tick remainder so the frame period
            // stays tight and the FPS reads as stable rather than jittery.
            int64_t ms = sleep_time_us / 1000;
            if (ms > 1) vTaskDelay(pdMS_TO_TICKS(ms - 1));
            while (esp_timer_get_time() < target_end_us) { /* busy-wait */ }
        }
    }
}

float yr_get_frame_time() {
    return cached_frame_time;
}

float yr_get_time() {
    return esp_timer_get_time() / 1000000.0f; // Return time in seconds
}

float yr_get_fps() {
    if (cached_frame_time <= 0.0f) return 0.0f;
    return 1.0f / cached_frame_time;
}
