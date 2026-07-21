#include <raylib.h>
#include "../../inputs.h"
#include <stdbool.h>

bool yr_is_key_down(int key) {
    return IsKeyDown(key);
}

bool yr_is_key_up(int key) {
    return IsKeyUp(key);
}

bool yr_is_key_pressed(int key) {
    return IsKeyPressed(key);
}

void yr_inputs_init() {}
void yr_esp_key_init(int pin, int key) {
    (void)pin;
    (void)key;
}

int yr_esp_joystick_init(int joystick_pin_x, int joystick_pin_y) {
    (void)joystick_pin_x;
    (void)joystick_pin_y;
    return -1;
}
float yr_esp_joystick_get_axis(int joystick_id, int axis) {
    (void)joystick_id;
    (void)axis;
    return 0;
}
