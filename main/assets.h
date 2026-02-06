#ifndef ASSETS_H
#define ASSETS_H
#include <stdint.h>
#include <stddef.h>

#ifdef ESP32
typedef uint16_t pixel_t;
#else
typedef uint32_t pixel_t;
#endif

// bricks.png
typedef enum {
    NULL_ASSET,
    tx_bricks,
} TextureId;

const pixel_t *assets_map[] = {
    NULL,
    bricks,
};
