#include <stdint.h>
#include <esp_timer.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#define RAYMATH_STATIC_INLINE
#define ESP32
#include "raymath.h"

// =================== CONFIG ===================

// T-Display (ST7789) pins
#define PIN_MOSI 19
#define PIN_CLK 18
#define PIN_CS 5
#define PIN_DC 16
#define PIN_RST 23
#define PIN_BL 4
#define MAX_TRANSFER_SIZE 65536

// T-Display buttons
#define PIN_BUTTON_LEFT 0
#define PIN_BUTTON_RIGHT 35

// Panel size (T-Display active area)
#define LCD_H 140
#define LCD_W 240

#define CLITERAL(type) (type)

#define LIGHTGRAY CLITERAL(Color){200, 200, 200, 255} // Light Gray
#define GRAY CLITERAL(Color){130, 130, 130, 255}      // Gray
#define DARKGRAY CLITERAL(Color){80, 80, 80, 255}     // Dark Gray
#define YELLOW CLITERAL(Color){253, 249, 0, 255}      // Yellow
#define GOLD CLITERAL(Color){255, 203, 0, 255}        // Gold
#define ORANGE CLITERAL(Color){255, 161, 0, 255}      // Orange
#define PINK CLITERAL(Color){255, 109, 194, 255}      // Pink
#define RED CLITERAL(Color){230, 41, 55, 255}         // Red
#define MAROON CLITERAL(Color){190, 33, 55, 255}      // Maroon
#define GREEN CLITERAL(Color){0, 228, 48, 255}        // Green
#define LIME CLITERAL(Color){0, 158, 47, 255}         // Lime
#define DARKGREEN CLITERAL(Color){0, 117, 44, 255}    // Dark Green
#define SKYBLUE CLITERAL(Color){102, 191, 255, 255}   // Sky Blue
#define BLUE CLITERAL(Color){0, 121, 241, 255}        // Blue
#define DARKBLUE CLITERAL(Color){0, 82, 172, 255}     // Dark Blue
#define PURPLE CLITERAL(Color){200, 122, 255, 255}    // Purple
#define VIOLET CLITERAL(Color){135, 60, 190, 255}     // Violet
#define DARKPURPLE CLITERAL(Color){112, 31, 126, 255} // Dark Purple
#define BEIGE CLITERAL(Color){211, 176, 131, 255}     // Beige
#define BROWN CLITERAL(Color){127, 106, 79, 255}      // Brown
#define DARKBROWN CLITERAL(Color){76, 63, 47, 255}    // Dark Brown

#define WHITE CLITERAL(Color){255, 255, 255, 255}    // White
#define BLACK CLITERAL(Color){0, 0, 0, 255}          // Black
#define BLANK CLITERAL(Color){0, 0, 0, 0}            // Blank (Transparent)
#define MAGENTA CLITERAL(Color){255, 0, 255, 255}    // Magenta
#define RAYWHITE CLITERAL(Color){245, 245, 245, 255} // My own White (raylib logo)

typedef struct Color {
    unsigned char r; // Color red value
    unsigned char g; // Color green value
    unsigned char b; // Color blue value
    unsigned char a; // Color alpha value
} Color;


typedef enum {
    KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    KEY_APOSTROPHE      = 39,       // Key: '
    KEY_COMMA           = 44,       // Key: ,
    KEY_MINUS           = 45,       // Key: -
    KEY_PERIOD          = 46,       // Key: .
    KEY_SLASH           = 47,       // Key: /
    KEY_ZERO            = 48,       // Key: 0
    KEY_ONE             = 49,       // Key: 1
    KEY_TWO             = 50,       // Key: 2
    KEY_THREE           = 51,       // Key: 3
    KEY_FOUR            = 52,       // Key: 4
    KEY_FIVE            = 53,       // Key: 5
    KEY_SIX             = 54,       // Key: 6
    KEY_SEVEN           = 55,       // Key: 7
    KEY_EIGHT           = 56,       // Key: 8
    KEY_NINE            = 57,       // Key: 9
    KEY_SEMICOLON       = 59,       // Key: ;
    KEY_EQUAL           = 61,       // Key: =
    KEY_A               = 65,       // Key: A | a
    KEY_B               = 66,       // Key: B | b
    KEY_C               = 67,       // Key: C | c
    KEY_D               = 68,       // Key: D | d
    KEY_E               = 69,       // Key: E | e
    KEY_F               = 70,       // Key: F | f
    KEY_G               = 71,       // Key: G | g
    KEY_H               = 72,       // Key: H | h
    KEY_I               = 73,       // Key: I | i
    KEY_J               = 74,       // Key: J | j
    KEY_K               = 75,       // Key: K | k
    KEY_L               = 76,       // Key: L | l
    KEY_M               = 77,       // Key: M | m
    KEY_N               = 78,       // Key: N | n
    KEY_O               = 79,       // Key: O | o
    KEY_P               = 80,       // Key: P | p
    KEY_Q               = 81,       // Key: Q | q
    KEY_R               = 82,       // Key: R | r
    KEY_S               = 83,       // Key: S | s
    KEY_T               = 84,       // Key: T | t
    KEY_U               = 85,       // Key: U | u
    KEY_V               = 86,       // Key: V | v
    KEY_W               = 87,       // Key: W | w
    KEY_X               = 88,       // Key: X | x
    KEY_Y               = 89,       // Key: Y | y
    KEY_Z               = 90,       // Key: Z | z
    KEY_LEFT_BRACKET    = 91,       // Key: [
    KEY_BACKSLASH       = 92,       // Key: '\'
    KEY_RIGHT_BRACKET   = 93,       // Key: ]
    KEY_GRAVE           = 96,       // Key: `
    // Function keys
    KEY_SPACE           = 32,       // Key: Space
    KEY_ESCAPE          = 256,      // Key: Esc
    KEY_ENTER           = 257,      // Key: Enter
    KEY_TAB             = 258,      // Key: Tab
    KEY_BACKSPACE       = 259,      // Key: Backspace
    KEY_INSERT          = 260,      // Key: Ins
    KEY_DELETE          = 261,      // Key: Del
    KEY_RIGHT           = 262,      // Key: Cursor right
    KEY_LEFT            = 263,      // Key: Cursor left
    KEY_DOWN            = 264,      // Key: Cursor down
    KEY_UP              = 265,      // Key: Cursor up
    KEY_PAGE_UP         = 266,      // Key: Page up
    KEY_PAGE_DOWN       = 267,      // Key: Page down
    KEY_HOME            = 268,      // Key: Home
    KEY_END             = 269,      // Key: End
    KEY_CAPS_LOCK       = 280,      // Key: Caps lock
    KEY_SCROLL_LOCK     = 281,      // Key: Scroll down
    KEY_NUM_LOCK        = 282,      // Key: Num lock
    KEY_PRINT_SCREEN    = 283,      // Key: Print screen
    KEY_PAUSE           = 284,      // Key: Pause
    KEY_F1              = 290,      // Key: F1
    KEY_F2              = 291,      // Key: F2
    KEY_F3              = 292,      // Key: F3
    KEY_F4              = 293,      // Key: F4
    KEY_F5              = 294,      // Key: F5
    KEY_F6              = 295,      // Key: F6
    KEY_F7              = 296,      // Key: F7
    KEY_F8              = 297,      // Key: F8
    KEY_F9              = 298,      // Key: F9
    KEY_F10             = 299,      // Key: F10
    KEY_F11             = 300,      // Key: F11
    KEY_F12             = 301,      // Key: F12
    KEY_LEFT_SHIFT      = 340,      // Key: Shift left
    KEY_LEFT_CONTROL    = 341,      // Key: Control left
    KEY_LEFT_ALT        = 342,      // Key: Alt left
    KEY_LEFT_SUPER      = 343,      // Key: Super left
    KEY_RIGHT_SHIFT     = 344,      // Key: Shift right
    KEY_RIGHT_CONTROL   = 345,      // Key: Control right
    KEY_RIGHT_ALT       = 346,      // Key: Alt right
    KEY_RIGHT_SUPER     = 347,      // Key: Super right
    KEY_KB_MENU         = 348,      // Key: KB menu
    // Keypad keys
    KEY_KP_0            = 320,      // Key: Keypad 0
    KEY_KP_1            = 321,      // Key: Keypad 1
    KEY_KP_2            = 322,      // Key: Keypad 2
    KEY_KP_3            = 323,      // Key: Keypad 3
    KEY_KP_4            = 324,      // Key: Keypad 4
    KEY_KP_5            = 325,      // Key: Keypad 5
    KEY_KP_6            = 326,      // Key: Keypad 6
    KEY_KP_7            = 327,      // Key: Keypad 7
    KEY_KP_8            = 328,      // Key: Keypad 8
    KEY_KP_9            = 329,      // Key: Keypad 9
    KEY_KP_DECIMAL      = 330,      // Key: Keypad .
    KEY_KP_DIVIDE       = 331,      // Key: Keypad /
    KEY_KP_MULTIPLY     = 332,      // Key: Keypad *
    KEY_KP_SUBTRACT     = 333,      // Key: Keypad -
    KEY_KP_ADD          = 334,      // Key: Keypad +
    KEY_KP_ENTER        = 335,      // Key: Keypad Enter
    KEY_KP_EQUAL        = 336,      // Key: Keypad =
    // Android key buttons
    KEY_BACK            = 4,        // Key: Android back button
    KEY_MENU            = 5,        // Key: Android menu button
    KEY_VOLUME_UP       = 24,       // Key: Android volume up button
    KEY_VOLUME_DOWN     = 25        // Key: Android volume down button
} KeyboardKey;


typedef enum {
    MOUSE_BUTTON_LEFT    = 0,       // Mouse button left
    MOUSE_BUTTON_RIGHT   = 1,       // Mouse button right
    MOUSE_BUTTON_MIDDLE  = 2,       // Mouse button middle (pressed wheel)
    MOUSE_BUTTON_SIDE    = 3,       // Mouse button side (advanced mouse device)
    MOUSE_BUTTON_EXTRA   = 4,       // Mouse button extra (advanced mouse device)
    MOUSE_BUTTON_FORWARD = 5,       // Mouse button forward (advanced mouse device)
    MOUSE_BUTTON_BACK    = 6,       // Mouse button back (advanced mouse device)
} MouseButton;

static int64_t last_time_us = 0;
static int64_t frame_start_time_us = 0;
static int target_fps = 0;
static int64_t target_frame_time_us = 0;


// ST7789 RAM offsets
static int g_x_off = 52;
static int g_y_off = 40;
static int g_w = LCD_W;
static int g_h = LCD_H;

static uint16_t framebuffer[LCD_W * LCD_H];

static inline uint16_t color_to_rgb565(Color c) {
    uint16_t rgb = (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
    return (rgb >> 8) | (rgb << 8);
}

// =================== SPI/LCD ===================

static spi_device_handle_t spi;

static void lcd_cmd(uint8_t cmd) {
    gpio_set_level(PIN_DC, 0);
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &cmd;
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
    x0 += g_x_off;
    x1 += g_x_off;
    y0 += g_y_off;
    y1 += g_y_off;

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


// =================== PUBLIC API ===================

void ClearBackground(Color color) {
    uint16_t rgb565_color = color_to_rgb565(color);
    for (int i = 0; i < LCD_W * LCD_H; i++) {
        framebuffer[i] = rgb565_color;
    }
}

void DrawRectangle(int posX, int posY, int width, int height, Color color) {
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

    uint16_t rgb565_color = color_to_rgb565(color);
    
    for (int y = 0; y < height; y++) {
        int lCD_offset = (posY + y) * LCD_W + posX;
        for (int x = 0; x < width; x++) {
            framebuffer[lCD_offset + x] = rgb565_color;
        }
    }
}

// =================== INIT ===================

void lcd_init(void) {
    gpio_set_direction(PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BL, GPIO_MODE_OUTPUT);
    
    gpio_set_direction(PIN_BUTTON_LEFT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_LEFT, GPIO_PULLUP_ONLY);
    gpio_set_direction(PIN_BUTTON_RIGHT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON_RIGHT, GPIO_PULLUP_ONLY);

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
        .max_transfer_sz = MAX_TRANSFER_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 80 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY
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
    g_w = LCD_W;
    g_h = LCD_H;
    g_x_off = 40;
    g_y_off = 52;
    lcd_cmd(0x36);
    lcd_data(&madctl, 1);

    lcd_cmd(0x21); // INVON
    lcd_cmd(0x13); // NORON
    lcd_cmd(0x29); // DISPON

    gpio_set_level(PIN_BL, 1);
}

void InitWindow(int width, int height, const char *title) {
    (void)width;
    (void)height;
    (void)title;
    lcd_init();
}

int WindowShouldClose() {
    return 0;
}

void SetTargetFPS(int fps) {
    target_fps = fps;
    if (fps > 0) {
        target_frame_time_us = 1000000 / fps;
    } else {
        target_frame_time_us = 0;
    }
}

void BeginDrawing() {
    frame_start_time_us = esp_timer_get_time();
}

void EndDrawing() {
    vTaskDelay(pdMS_TO_TICKS(2));
    
    lcd_set_window(0, 0, LCD_W - 1, LCD_H - 1);
    gpio_set_level(PIN_DC, 1);
    
    const int max_bytes_per_chunk = MAX_TRANSFER_SIZE;
    const int total_bytes = LCD_W * LCD_H * 2;
    int bytes_sent = 0;
    
    while (bytes_sent < total_bytes) {
        int bytes_to_send = total_bytes - bytes_sent;
        if (bytes_to_send > max_bytes_per_chunk) {
            bytes_to_send = max_bytes_per_chunk;
        }
        
        spi_transaction_t t = {0};
        t.length = bytes_to_send * 8;
        t.tx_buffer = ((uint8_t*)framebuffer) + bytes_sent;
        
        ESP_ERROR_CHECK(spi_device_transmit(spi, &t));
        
        bytes_sent += bytes_to_send;
    }
    
    if (target_fps > 0) {
        int64_t frame_end_time_us = esp_timer_get_time();
        int64_t frame_elapsed_us = frame_end_time_us - frame_start_time_us;
        int64_t sleep_time_us = target_frame_time_us - frame_elapsed_us;
        
        if (sleep_time_us > 0) {
            vTaskDelay(pdMS_TO_TICKS(sleep_time_us / 1000));
        }
    }
}


float GetFrameTime(void){
    int64_t current_time_us = esp_timer_get_time();
    if (last_time_us == 0)
    {
        last_time_us = current_time_us;
        return 0.0f;
    }

    int64_t delta_us = current_time_us - last_time_us;
    last_time_us = current_time_us;

    return (float)delta_us / 1000000.0f;
}


int IsKeyDown(KeyboardKey key) {
    if (key == KEY_E) {
        return !gpio_get_level(PIN_BUTTON_RIGHT) && !gpio_get_level(PIN_BUTTON_LEFT);
    }
    if (key == KEY_A) {
        return !gpio_get_level(PIN_BUTTON_LEFT);
    }
    if (key == KEY_D) {
        return !gpio_get_level(PIN_BUTTON_RIGHT);
    }
    return 0;
}

Color ColorBrightness(Color color, float factor)
{
    Color result = color;

    if (factor > 1.0f) factor = 1.0f;
    else if (factor < -1.0f) factor = -1.0f;

    float red = (float)color.r;
    float green = (float)color.g;
    float blue = (float)color.b;

    if (factor < 0.0f)
    {
        factor = 1.0f + factor;
        red *= factor;
        green *= factor;
        blue *= factor;
    }
    else
    {
        red = (255 - red)*factor + red;
        green = (255 - green)*factor + green;
        blue = (255 - blue)*factor + blue;
    }

    result.r = (unsigned char)red;
    result.g = (unsigned char)green;
    result.b = (unsigned char)blue;

    return result;
}

#define FLAG_MSAA_4X_HINT 0
void SetConfigFlags(int flags) {}

void DrawRectangleLines(int x, int y, int width, int height, Color color) {}
void DrawLine(int startX, int startY, int endX, int endY, Color color) {}
void DrawCircleV(Vector2 center, float radius, Color color) {}
void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color) {}