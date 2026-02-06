# Raycast engine for ESP32 with T-display
This is a simple raycasting engine implemented in C for the ESP32 microcontroller, designed to work with the ST7789 display driver. 
It also compiles for other platforms with the raylib library.

## Installation
#### ESP32
To use this code, you need to have the ESP-IDF development environment set up on your machine.
Follow the official ESP-IDF documentation to install and configure the environment: [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
The development has been done using ESP-IDF v5.5.2

#### MacOS/Linux
To compile and run the code on other platforms, you need to have the raylib library installed.
Follow the official raylib documentation to install and configure the library: [Raylib Installation Guide](https://github.com/raysan5/raylib)
The development has been done using raylib v5.5

Windows is not supported at the moment.

To compile the code and run it, use the following command:
```sh
make run
```

## Compilation flags

```c
#define FB_DRAM      // Define this flag to place the framebuffer in DRAM instead of IRAM.

// LCD configuration
#define LCD_W 240    // Active width of the display
#define LCD_H 135    // Active height of the display
// internal vram is usually 320×240 for T-Display, smaller screens should be offsetted
#define LCD_X_OFF 40 // display vram X offset
#define LCD_Y_OFF 53 // display vram Y offset

// LCD Pins
#define PIN_MOSI 19  // SPI MOSI pin
#define PIN_CLK 18   // SPI Clock pin
#define PIN_CS 5     // SPI Chip Select pin
#define PIN_DC 16    // SPI Data/Command pin
#define PIN_RST 23   // SPI Reset pin
#define PIN_BL 4     // SPI Backlight pin
#define SPI_CLOCK_SPEED (80 * 1000 * 1000) // SPI clock speed in Hz

// Key Pins
#define PIN_KEY_A 0  // Key A pin
#define PIN_KEY_D 35 // Key D pin
```