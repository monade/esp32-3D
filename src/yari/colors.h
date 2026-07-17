#ifndef YR_COLORS_H
#define YR_COLORS_H
#include <stdint.h>



#if defined(COLOR_565) // 16-bit color in 5-6-5 format
typedef uint16_t yr_pixel_t;

#define YR_COLOR(r, g, b) ((yr_pixel_t)(((int)((r)*31) << 11) | ((int)((g)*63) << 5) | (int)((b)*31))) 

#define YR_EMPTY_PIXEL 0
#define YR_BLACK       0x0001
#define YR_WHITE       0xFFFF
#define YR_RED         0xF800
#define YR_GREEN       0x07E0
#define YR_BLUE        0x001F
#define YR_YELLOW      0xFFE0
#define YR_PURPLE      0xF81F
#define YR_ORANGE      0xFC00
#define YR_CYAN        0x07FF
#define YR_PINK        0xFE19
#define YR_GRAY        0x8410
#define YR_SILVER      0xC618
#define YR_MAROON      0x8000
#define YR_DARK_RED    0x8800
#define YR_DARK_GREEN  0x0400
#define YR_DARK_BLUE   0x0011
#define YR_OLIVE       0x8400
#define YR_TEAL        0x0410
#define YR_NAVY        0x0010
#define YR_BROWN       0xA145
#define YR_SKY_BLUE    0x865D

static inline yr_pixel_t yr_color_darken(yr_pixel_t color, int scale) {
    if (scale <= 0) return 0;
    if (scale >= 256) return color;

    int red = color >> 11;
    int green = (color >> 5) & 0x3F;
    int blue = color & 0x1F;

    red = (red * scale + 128) >> 8;
    green = (green * scale) >> 8;
    blue = (blue * scale + 128) >> 8;

    return (red << 11) | (green << 5) | blue;
}

static inline yr_pixel_t yr_color_brightness(yr_pixel_t color, float factor) {
    int red, green, blue;

    if (factor < 0.0f) {
        if (factor <= -1.0f) return 0;

        int scale = (int)((1.0f + factor) * 256.0f);
        return yr_color_darken(color, scale);
    } else {
        if (factor >= 1.0f) return YR_WHITE;
        red = color >> 11;
        green = (color >> 5) & 0x3F;
        blue = color & 0x1F;
        int scale = (int)(factor * 256.0f);
        red += ((31 - red) * scale) >> 8;
        green += ((63 - green) * scale) >> 8;
        blue += ((31 - blue) * scale) >> 8;
    }

    return (red << 11) | (green << 5) | blue;
}
#elif defined(COLOR_MONO) // 8-bit grayscale; dithered to 1bpp only at the final display blit
typedef uint8_t yr_pixel_t;

#define YR_COLOR(r, g, b) ((yr_pixel_t)(int)(((r) * 0.299f + (g) * 0.587f + (b) * 0.114f) * 255))

#define YR_EMPTY_PIXEL 0
#define YR_BLACK       1
#define YR_WHITE       255
#define YR_RED         76
#define YR_GREEN       150
#define YR_BLUE        29
#define YR_YELLOW      226
#define YR_PURPLE      105
#define YR_ORANGE      151
#define YR_CYAN        179
#define YR_PINK        212
#define YR_GRAY        128
#define YR_SILVER      192
#define YR_MAROON      38
#define YR_DARK_RED    42
#define YR_DARK_GREEN  75
#define YR_DARK_BLUE   16
#define YR_OLIVE       113
#define YR_TEAL        90
#define YR_NAVY        15
#define YR_BROWN       79
#define YR_SKY_BLUE    188

static inline yr_pixel_t yr_color_darken(yr_pixel_t color, int scale) {
    if (scale <= 0) return 0;
    if (scale >= 256) return color;

    return (yr_pixel_t)((color * scale) >> 8);
}

static inline yr_pixel_t yr_color_brightness(yr_pixel_t color, float factor) {
    if (factor < 0.0f) {
        if (factor <= -1.0f) return 0;

        int scale = (int)((1.0f + factor) * 256.0f);
        return yr_color_darken(color, scale);
    } else {
        if (factor >= 1.0f) return YR_WHITE;

        int scale = (int)(factor * 256.0f);
        return (yr_pixel_t)(color + (((255 - color) * scale) >> 8));
    }
}
#else // 32-bit color with alpha, in ARGB format
typedef uint32_t yr_pixel_t;

#define YR_COLOR(r, g, b) ((yr_pixel_t)(((int)((r)*255) << 24) | ((int)((g)*255) << 16) | ((int)((b)*255) << 8) | 0xFF)) 

#define YR_EMPTY_PIXEL 0xFF
#define YR_BLACK       0x000001FF
#define YR_WHITE       0xFFFFFFFF
#define YR_RED         0xFF0000FF
#define YR_GREEN       0x00FF00FF
#define YR_BLUE        0x0000FFFF
#define YR_YELLOW      0xFFFF00FF
#define YR_PURPLE      0xFF00FFFF
#define YR_ORANGE      0xFF8000FF
#define YR_CYAN        0x00FFFFFF
#define YR_PINK        0xFFC0CBFF
#define YR_GRAY        0x808080FF
#define YR_SILVER      0xC0C0C0FF
#define YR_MAROON      0x800000FF
#define YR_DARK_RED    0x8B0000FF
#define YR_DARK_GREEN  0x008000FF
#define YR_DARK_BLUE   0x00008BFF
#define YR_OLIVE       0x808000FF
#define YR_TEAL        0x008080FF
#define YR_NAVY        0x000080FF
#define YR_BROWN       0xA52A2AFF
#define YR_SKY_BLUE    0x87CEEBFF

static inline yr_pixel_t yr_color_darken(yr_pixel_t color, int scale) {
    if (scale <= 0) return color & 0xFF;
    if (scale >= 256) return color;

    int red = color >> 24;
    int green = (color >> 16) & 0xFF;
    int blue = (color >> 8) & 0xFF;

    red = (red * scale + 128) >> 8;
    green = (green * scale + 128) >> 8;
    blue = (blue * scale + 128) >> 8;

    return (red << 24) | (green << 16) | (blue << 8) | (color & 0xFF);
}

static inline yr_pixel_t yr_color_brightness(yr_pixel_t color, float factor) {
    if (factor > 1.0f) factor = 1.0f;
    else if (factor < -1.0f) factor = -1.0f;

    float red, green, blue;

    if (factor < 0.0f) {
        int scale = (int)((1.0f + factor) * 256.0f);
        return yr_color_darken(color, scale);
    } else {
        red = color >> 24;
        green = (color >> 16) & 0xFF;
        blue = (color >> 8) & 0xFF;
        red = (255 - red) * factor + red;
        green = (255 - green) * factor + green;
        blue = (255 - blue) * factor + blue;
    }

    return ((int)red << 24) | ((int)green << 16) | ((int)blue << 8) | 0xFF;
}
#endif

#ifdef YARI_NO_PREFIX
#define color_brightness yr_color_brightness
#define color_darken yr_color_darken
#define pixel_t yr_pixel_t
#endif

#endif // YR_COLORS_H
