#ifndef YR_INPUTS_H
#define YR_INPUTS_H

#include <stdbool.h>

typedef enum {
    YR_KEY_NULL            = 0,        // Key: NULL, used for no key pressed
    // Alphanumeric keys
    YR_KEY_APOSTROPHE      = 39,       // Key: '
    YR_KEY_COMMA           = 44,       // Key: ,
    YR_KEY_MINUS           = 45,       // Key: -
    YR_KEY_PERIOD          = 46,       // Key: .
    YR_KEY_SLASH           = 47,       // Key: /
    YR_KEY_ZERO            = 48,       // Key: 0
    YR_KEY_ONE             = 49,       // Key: 1
    YR_KEY_TWO             = 50,       // Key: 2
    YR_KEY_THREE           = 51,       // Key: 3
    YR_KEY_FOUR            = 52,       // Key: 4
    YR_KEY_FIVE            = 53,       // Key: 5
    YR_KEY_SIX             = 54,       // Key: 6
    YR_KEY_SEVEN           = 55,       // Key: 7
    YR_KEY_EIGHT           = 56,       // Key: 8
    YR_KEY_NINE            = 57,       // Key: 9
    YR_KEY_SEMICOLON       = 59,       // Key: ;
    YR_KEY_EQUAL           = 61,       // Key: =
    YR_KEY_A               = 65,       // Key: A | a
    YR_KEY_B               = 66,       // Key: B | b
    YR_KEY_C               = 67,       // Key: C | c
    YR_KEY_D               = 68,       // Key: D | d
    YR_KEY_E               = 69,       // Key: E | e
    YR_KEY_F               = 70,       // Key: F | f
    YR_KEY_G               = 71,       // Key: G | g
    YR_KEY_H               = 72,       // Key: H | h
    YR_KEY_I               = 73,       // Key: I | i
    YR_KEY_J               = 74,       // Key: J | j
    YR_KEY_K               = 75,       // Key: K | k
    YR_KEY_L               = 76,       // Key: L | l
    YR_KEY_M               = 77,       // Key: M | m
    YR_KEY_N               = 78,       // Key: N | n
    YR_KEY_O               = 79,       // Key: O | o
    YR_KEY_P               = 80,       // Key: P | p
    YR_KEY_Q               = 81,       // Key: Q | q
    YR_KEY_R               = 82,       // Key: R | r
    YR_KEY_S               = 83,       // Key: S | s
    YR_KEY_T               = 84,       // Key: T | t
    YR_KEY_U               = 85,       // Key: U | u
    YR_KEY_V               = 86,       // Key: V | v
    YR_KEY_W               = 87,       // Key: W | w
    YR_KEY_X               = 88,       // Key: X | x
    YR_KEY_Y               = 89,       // Key: Y | y
    YR_KEY_Z               = 90,       // Key: Z | z
    YR_KEY_LEFT_BRACKET    = 91,       // Key: [
    YR_KEY_BACKSLASH       = 92,       // Key: '\'
    YR_KEY_RIGHT_BRACKET   = 93,       // Key: ]
    YR_KEY_GRAVE           = 96,       // Key: `
    // Function keys
    YR_KEY_SPACE           = 32,       // Key: Space
    YR_KEY_ESCAPE          = 256,      // Key: Esc
    YR_KEY_ENTER           = 257,      // Key: Enter
    YR_KEY_TAB             = 258,      // Key: Tab
    YR_KEY_BACKSPACE       = 259,      // Key: Backspace
    YR_KEY_INSERT          = 260,      // Key: Ins
    YR_KEY_DELETE          = 261,      // Key: Del
    YR_KEY_RIGHT           = 262,      // Key: Cursor right
    YR_KEY_LEFT            = 263,      // Key: Cursor left
    YR_KEY_DOWN            = 264,      // Key: Cursor down
    YR_KEY_UP              = 265,      // Key: Cursor up
    YR_KEY_PAGE_UP         = 266,      // Key: Page up
    YR_KEY_PAGE_DOWN       = 267,      // Key: Page down
    YR_KEY_HOME            = 268,      // Key: Home
    YR_KEY_END             = 269,      // Key: End
    YR_KEY_CAPS_LOCK       = 280,      // Key: Caps lock
    YR_KEY_SCROLL_LOCK     = 281,      // Key: Scroll down
    YR_KEY_NUM_LOCK        = 282,      // Key: Num lock
    YR_KEY_PRINT_SCREEN    = 283,      // Key: Print screen
    YR_KEY_PAUSE           = 284,      // Key: Pause
    YR_KEY_F1              = 290,      // Key: F1
    YR_KEY_F2              = 291,      // Key: F2
    YR_KEY_F3              = 292,      // Key: F3
    YR_KEY_F4              = 293,      // Key: F4
    YR_KEY_F5              = 294,      // Key: F5
    YR_KEY_F6              = 295,      // Key: F6
    YR_KEY_F7              = 296,      // Key: F7
    YR_KEY_F8              = 297,      // Key: F8
    YR_KEY_F9              = 298,      // Key: F9
    YR_KEY_F10             = 299,      // Key: F10
    YR_KEY_F11             = 300,      // Key: F11
    YR_KEY_F12             = 301,      // Key: F12
    YR_KEY_LEFT_SHIFT      = 340,      // Key: Shift left
    YR_KEY_LEFT_CONTROL    = 341,      // Key: Control left
    YR_KEY_LEFT_ALT        = 342,      // Key: Alt left
    YR_KEY_LEFT_SUPER      = 343,      // Key: Super left
    YR_KEY_RIGHT_SHIFT     = 344,      // Key: Shift right
    YR_KEY_RIGHT_CONTROL   = 345,      // Key: Control right
    YR_KEY_RIGHT_ALT       = 346,      // Key: Alt right
    YR_KEY_RIGHT_SUPER     = 347,      // Key: Super right
    YR_KEYS_COUNT                      // the number of configured keys
} YrKeyboardKey;

typedef enum {
    YR_X_AXIS = 0,
    YR_Y_AXIS = 1,
} YrJoystickAxis;

void yr_inputs_init();
void yr_esp_key_init(int pin, int key);
int yr_esp_joystick_init(int joystick_pin_x, int joystick_pin_y);
void yr_esp_key_init(int pin, int key);
float yr_esp_joystick_get_axis(int joystick_id, int axis);
bool yr_is_key_down(int key);
bool yr_is_key_up(int key);
bool yr_is_key_pressed(int key);

#ifdef YARI_NO_PREFIX
#define JoystickConfig YrJoystickConfig
#define inputs_init yr_inputs_init
#define esp_key_init yr_esp_key_init
#define esp_joystick_init yr_esp_joystick_init
#define esp_joystick_get_axis yr_esp_joystick_get_axis
#define is_key_down yr_is_key_down
#define is_key_up yr_is_key_up
#define is_key_pressed yr_is_key_pressed
#endif

#endif // YR_INPUTS_H
