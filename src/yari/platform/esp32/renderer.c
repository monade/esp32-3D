#include <stdint.h>
#include <string.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_io_spi.h"
#include "renderer.h"
#include "colors.h"

// Panel active-area offset (gap between the controller's GRAM origin and the
// visible glass, e.g. 40/53 on the T-Display's ST7789)
#ifndef LCD_X_OFF
#define LCD_X_OFF 40
#endif
#ifndef LCD_Y_OFF
#define LCD_Y_OFF 53
#endif

// SPI pins
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
// Backlight is optional: define PIN_BL to -1 (or just don't drive it) for
// panels whose backlight isn't GPIO-controlled.
#ifndef PIN_BL
#define PIN_BL 4
#endif
#ifndef LCD_BL_ACTIVE_LOW
#define LCD_BL_ACTIVE_LOW false
#endif

#ifndef LCD_SPI_HOST
#define LCD_SPI_HOST SPI2_HOST
#endif
#ifndef LCD_SPI_MODE
#define LCD_SPI_MODE 0
#endif

// Controller selection. Add another esp_lcd-compatible driver by defining
// its own LCD_CONTROLLER_xxx and wiring it into lcd_init()/LCD_BITS_PER_PIXEL
// below - everything else in this file (bus setup, rotation, framebuffer,
// blit/pack) is controller-agnostic.
#if !defined(LCD_CONTROLLER_ST7789) && !defined(LCD_CONTROLLER_SSD1306)
#define LCD_CONTROLLER_ST7789
#endif

#ifndef SPI_CLOCK_SPEED
#if defined(LCD_CONTROLLER_SSD1306)
#define SPI_CLOCK_SPEED (10 * 1000 * 1000)
#else
#define SPI_CLOCK_SPEED (80 * 1000 * 1000)
#endif
#endif

// Rotation/mirroring, applied through esp_lcd's portable panel ops so it
// works the same way regardless of which controller is selected. Defaults
// reproduce this project's original T-Display (ST7789) landscape orientation.
#ifndef LCD_MIRROR_X
#define LCD_MIRROR_X true
#endif
#ifndef LCD_MIRROR_Y
#define LCD_MIRROR_Y false
#endif
#ifndef LCD_SWAP_XY
#define LCD_SWAP_XY true
#endif
#ifndef LCD_BGR_ORDER
#define LCD_BGR_ORDER false
#endif
#ifndef LCD_INVERT_COLOR
// Most small ST7789 modules need INVON to show correct colors; other
// controllers default to the non-inverted, natural reading.
#if defined(LCD_CONTROLLER_ST7789)
#define LCD_INVERT_COLOR true
#else
#define LCD_INVERT_COLOR false
#endif
#endif

// Panel color depth. 1 = monochrome (dithered, packed per the controller's
// native GDDRAM layout), 8 = grayscale (YR_L8, direct passthrough), 16 =
// RGB565 (the fast, zero-copy path), anything else is treated as N
// bytes/pixel and filled by expanding RGB565 per channel. YR_MONOCROME and
// YR_L8 mirror the project-wide pixel-format switches of the same name (see
// colors.h): YR_MONOCROME always wants the dithered 1bpp wire path (even
// without a monochrome controller selected above), while YR_L8 alone means
// an actual grayscale panel.
#ifndef LCD_BITS_PER_PIXEL
#if defined(LCD_CONTROLLER_SSD1306) || defined(YR_MONOCROME)
#define LCD_BITS_PER_PIXEL 1
#elif defined(YR_L8)
#define LCD_BITS_PER_PIXEL 8
#else
#define LCD_BITS_PER_PIXEL 16
#endif
#endif

#define LCD_BYTES_PER_PIXEL ((LCD_BITS_PER_PIXEL + 7) / 8)
#if LCD_BITS_PER_PIXEL == 1
#define LCD_WIRE_BUFFER_SIZE ((YR_LCD_W * YR_LCD_H + 7) / 8)
#else
#define LCD_WIRE_BUFFER_SIZE (YR_LCD_W * YR_LCD_H * LCD_BYTES_PER_PIXEL)
#endif

#define SCREEN_PIXEL_COUNT (YR_LCD_W * YR_LCD_H)

static int64_t last_frame_start_us = 0;
static int64_t frame_start_time_us = 0;
static float cached_frame_time = 0.0f;
static int target_fps = 30;
static int64_t target_frame_time_us = 1000000 / 30;

// Canonical RGB565 framebuffer the engine draws into. Its byte order matches
// the wire format only in the 16bpp fast path (see lcd_color()); every other
// panel format is produced from this buffer once per frame in
// lcd_prepare_frame().
static uint16_t framebuffer0[SCREEN_PIXEL_COUNT];

static uint16_t *fb_back = framebuffer0;

// In the 16bpp fast path the framebuffer stores panel-order (byte-swapped)
// RGB565 pixels so a frame can be blitted verbatim with no per-frame
// conversion pass; other panel formats are converted once per frame instead
// (see lcd_prepare_frame()), so storage here just stays canonical RGB565.
static inline uint16_t lcd_color(yr_pixel_t color) {
    uint16_t c = (uint16_t)color;
#if LCD_BITS_PER_PIXEL == 16
    #ifdef ESP32_DISPLAY_LITTLE_ENDIAN
    return c;
    #else
    return (uint16_t)((c << 8) | (c >> 8));
    #endif
#else
    return c;
#endif
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

static esp_lcd_panel_io_handle_t lcd_io;
static esp_lcd_panel_handle_t lcd_panel;
static SemaphoreHandle_t lcd_trans_done_sem;

// Runs in the SPI driver's ISR context once the frame's color data has been
// fully clocked out, so yr_render_screen() can block until the framebuffer is
// safe to overwrite again.
static bool IRAM_ATTR lcd_on_color_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    (void)io;
    (void)edata;
    BaseType_t high_task_awoken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &high_task_awoken);
    return high_task_awoken == pdTRUE;
}

static void lcd_init(void) {
#if PIN_BL >= 0
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
#endif

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIRE_BUFFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    lcd_trans_done_sem = xSemaphoreCreateBinary();

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = PIN_DC,
        .spi_mode = LCD_SPI_MODE,
        .pclk_hz = SPI_CLOCK_SPEED,
        .trans_queue_depth = 1,
        .on_color_trans_done = lcd_on_color_trans_done,
        .user_ctx = lcd_trans_done_sem,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &lcd_io));

#if defined(LCD_CONTROLLER_SSD1306)
    esp_lcd_panel_ssd1306_config_t ssd1306_config = { .height = YR_LCD_H };
#endif
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_BGR_ORDER ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        #ifdef ESP32_DISPLAY_LITTLE_ENDIAN
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        #else
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        #endif
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        #if defined(LCD_CONTROLLER_SSD1306)
        .vendor_config = &ssd1306_config,
        #endif
    };

#if defined(LCD_CONTROLLER_SSD1306)
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(lcd_io, &panel_config, &lcd_panel));
#else
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel));
    // To support another esp_lcd-compatible controller (e.g. NT35510, or a
    // managed component such as ILI9341/GC9A01), add another #elif branch
    // here calling its esp_lcd_new_panel_xxx() constructor.
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, LCD_MIRROR_X, LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel, LCD_SWAP_XY));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd_panel, LCD_X_OFF, LCD_Y_OFF));

    if (LCD_INVERT_COLOR) {
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    }

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));

#if PIN_BL >= 0
    gpio_set_level(PIN_BL, LCD_BL_ACTIVE_LOW ? 0 : 1);
#endif
}

#if LCD_BITS_PER_PIXEL == 16
// Fast path: the framebuffer already stores panel-ready RGB565 words (see
// lcd_color()), so a frame is just blitted verbatim with no conversion pass.
static inline const void *lcd_prepare_frame(void) {
    return fb_back;
}
#elif LCD_BITS_PER_PIXEL == 1
static uint8_t lcd_wire_buffer[LCD_WIRE_BUFFER_SIZE];

#define LCD_LUMA_MAX ((31 * 8) + (63 * 4) + (31 * 2))
// Cheap RGB565 luminance approximation (weights are just the per-channel bit
// ranges, not a proper colorimetric formula), rescaled to 0..255 and handed
// to the shared Bayer-dithered threshold (see colors.h) so the panel gets
// the same ordered dithering as the YR_MONOCROME desktop simulation.
static inline bool lcd_pixel_is_lit(uint16_t c, int x, int y) {
    int luma = ((c >> 11) & 0x1F) * 8 + ((c >> 5) & 0x3F) * 4 + (c & 0x1F) * 2;
    return yr_mono_dither_lit((uint8_t)(luma * 255 / LCD_LUMA_MAX), x, y);
}

// Packs the canonical RGB565 framebuffer into the SSD1306/SH110x-style
// page-major 1bpp GDDRAM layout: each byte holds 8 vertically-stacked
// pixels (LSB = topmost row of its page), laid out page-by-page.
static const void *lcd_prepare_frame(void) {
    memset(lcd_wire_buffer, 0, sizeof(lcd_wire_buffer));
    for (int y = 0; y < YR_LCD_H; y++) {
        uint8_t *page = lcd_wire_buffer + (y / 8) * YR_LCD_W;
        uint8_t bit = (uint8_t)(1 << (y % 8));
        const uint16_t *src = fb_back + y * YR_LCD_W;
        for (int x = 0; x < YR_LCD_W; x++) {
            if (lcd_pixel_is_lit(src[x], x, y)) page[x] |= bit;
        }
    }
    return lcd_wire_buffer;
}
#elif LCD_BITS_PER_PIXEL == 8
static uint8_t lcd_wire_buffer[LCD_WIRE_BUFFER_SIZE];

// YR_L8 stores a plain 0..255 luma value per pixel widened into the
// framebuffer's uint16_t slots (see lcd_color()), so this is a straight
// byte copy - no RGB565 bit extraction needed, unlike the other panel
// formats below.
static const void *lcd_prepare_frame(void) {
    uint8_t *dst = lcd_wire_buffer;
    const uint16_t *src = fb_back;
    for (int i = 0; i < SCREEN_PIXEL_COUNT; i++, src++) *dst++ = (uint8_t)*src;
    return lcd_wire_buffer;
}
#else
static uint8_t lcd_wire_buffer[LCD_WIRE_BUFFER_SIZE];

// Expands the canonical RGB565 framebuffer to LCD_BYTES_PER_PIXEL bytes per
// pixel (e.g. RGB888 for 24bpp panels), replicating each channel's high bits
// into the low bits so gradients stay smooth. Any bytes beyond the 3rd
// (e.g. alpha on a 32bpp panel) are padded opaque.
static const void *lcd_prepare_frame(void) {
    uint8_t *dst = lcd_wire_buffer;
    const uint16_t *src = fb_back;
    for (int i = 0; i < SCREEN_PIXEL_COUNT; i++, src++) {
        uint16_t c = *src;
        uint8_t r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
        *dst++ = (uint8_t)((r5 << 3) | (r5 >> 2));
        *dst++ = (uint8_t)((g6 << 2) | (g6 >> 4));
        *dst++ = (uint8_t)((b5 << 3) | (b5 >> 2));
        for (int pad = 3; pad < LCD_BYTES_PER_PIXEL; pad++) *dst++ = 0xFF;
    }
    return lcd_wire_buffer;
}
#endif

int yr_screen_width(void) {
    return YR_LCD_W;
}

int yr_screen_height(void) {
    return YR_LCD_H;
}

// Precondition (guaranteed by yr_draw_rectangle in renderer_common.c): the
// whole [x, x+width) run at row y is in bounds. width == 1 skips
// fill_pixels's 32-bit batching, not worth it for a single pixel.
void IRAM_ATTR yr_fill_span(int x, int y, int width, yr_pixel_t color) {
    uint16_t *dst = fb_back + y * YR_LCD_W + x;
    if (width == 1) {
        *dst = lcd_color(color);
        return;
    }
    fill_pixels(dst, width, lcd_color(color));
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

    uint16_t *px = fb_back + y_start * YR_LCD_W;
    for (int y = y_start; y < y_end; y++) {
        for (int x = 0; x < YR_LCD_W; x++, px++) {
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
    yr_run_split(yr_filter_rows, &ctx, YR_LCD_H);
#else
    yr_filter_rows(&ctx, 0, YR_LCD_H);
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
    set_target_fps(target_fps);
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
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(lcd_panel, 0, 0, YR_LCD_W, YR_LCD_H, lcd_prepare_frame()));
    // Single framebuffer: block until the DMA transfer is done so the next
    // frame's drawing doesn't race the SPI read of this one.
    xSemaphoreTake(lcd_trans_done_sem, portMAX_DELAY);
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
