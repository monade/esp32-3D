#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "raylib.h"
#define RAYMATH_STATIC_INLINE
#include "raymath.h"

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))
#define SetPixel(x, y, color) DrawRectangle(x, y, 1, 1, color)

#ifdef ESP32
    #define TARGET_FPS 30
    #define SCREEN_W LCD_W
    #define SCREEN_H LCD_H
    #define RAY_RES 2
#else
    #define TARGET_FPS 60
    #define SCREEN_W 800
    #define SCREEN_H 600
    #define RAY_RES 1
#endif
#define COLS 10
#define ROWS 10
#define ASPECT_RATIO ((float)SCREEN_W / SCREEN_H)
#define MINIMAP_CELL_SCALE 20
#define FOV_ANGLE (PI / 3.5)
#define MAX_RENDER_DIST 20.0

#define PLAYER_ROTATION_SPEED 1.25
#define PLAYER_SPEED 2.5

#define POINT_R 2.5
#define LINE_THICKNESS 1.5

typedef struct {
    Vector2 pos;
    Vector2 dir;
} Player;

// 0 null, 1-127 texture_id, 128-255 color_id
static uint8_t map[ROWS][COLS] = {0};

// TODO texture_map

Color color_map[] = {
    RED,     // 128
    GREEN,   // 129
    BLUE,    // 130
    YELLOW,  // 131
    PURPLE,  // 132
    ORANGE,  // 133
    WHITE    // 134
};

void init_game() {
    map[1][3] = 128;
    map[1][4] = 131;
    map[1][5] = 129;
    map[2][5] = 133;
    map[3][4] = 129;
    map[3][5] = 132;

    map[7][7] = 130;
    map[8][8] = 129;
    map[9][9] = 134;
}

void draw_minimap() {
    DrawRectangle(0, 0, COLS * MINIMAP_CELL_SCALE, ROWS * MINIMAP_CELL_SCALE, GetColor(0x00000046));
    DrawRectangleLines(0, 0, COLS * MINIMAP_CELL_SCALE, ROWS * MINIMAP_CELL_SCALE, RAYWHITE);
    for (int i = 1; i < COLS; i++) {
        int x = i * MINIMAP_CELL_SCALE;
        DrawLine(x, 0, x, ROWS * MINIMAP_CELL_SCALE, RAYWHITE);
    }
    for (int i = 1; i < ROWS; i++) {
        int y = i * MINIMAP_CELL_SCALE;
        DrawLine(0, y, COLS * MINIMAP_CELL_SCALE, y, RAYWHITE);
    }
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            int ci = i*MINIMAP_CELL_SCALE;
            int cj = j*MINIMAP_CELL_SCALE;
            uint8_t c = map[i][j];
            if (c == 0) continue;
            if (c >= 128) {
                // color
                DrawRectangle(cj, ci, MINIMAP_CELL_SCALE, MINIMAP_CELL_SCALE, color_map[c - 128]);
            } else {
                // texture
                DrawRectangle(cj, ci, MINIMAP_CELL_SCALE, MINIMAP_CELL_SCALE, MAGENTA);
                DrawLine(cj, ci, cj + MINIMAP_CELL_SCALE, ci + MINIMAP_CELL_SCALE, BLACK);
                DrawLine(cj + MINIMAP_CELL_SCALE, ci, cj, ci + MINIMAP_CELL_SCALE, BLACK);
            }
        }
    }
}

void draw_minimap_player(Vector2 p) {
    DrawCircleV(Vector2Scale(p, MINIMAP_CELL_SCALE), POINT_R * 2.0, GREEN);
}

void raycast_walls(Player p, Vector2 dir, int slice_x) {
    if (dir.x == 0.0) dir.x = EPSILON;
    if (dir.y == 0.0) dir.y = EPSILON;
    Vector2 rs = Vector2Add(p.pos, Vector2Scale(dir, EPSILON));
    while (Vector2Length(Vector2Subtract(rs, p.pos)) <= MAX_RENDER_DIST) {
        Vector2 cell = {.x = floorf(rs.x), .y = floorf(rs.y)};
        if (rs.x > 0.0 && rs.x < COLS && rs.y > 0.0 && rs.y < ROWS) {
            uint8_t map_cell = map[(int)cell.y][(int)cell.x];
            if (map_cell) {
                // draw slice
                float dist = Vector2DotProduct(Vector2Subtract(rs, p.pos), p.dir) / ASPECT_RATIO;
                float h = SCREEN_H / dist;
                float bright_factor = 1.0 / dist - 0.9;
                if (bright_factor >= 0.0) bright_factor = 0.0;

                if (map_cell >= 128) {
                    // color
                    Color c = ColorBrightness(color_map[map_cell - 128], bright_factor);
                    DrawRectangle(slice_x, (SCREEN_H - h) / 2.0, RAY_RES, h, c);
                } else {
                    // texture
                }
                return;
            }
        }
        float distX = cell.x + (dir.x >= 0 ? 1.0 : -EPSILON) - rs.x;
        float distY = cell.y + (dir.y >= 0 ? 1.0 : -EPSILON) - rs.y;
        Vector2 inc;
        if (fabs(distX / dir.x) < fabs(distY / dir.y)) {
            inc = (Vector2){.x = distX, .y = distX * dir.y / dir.x};
        } else {
            inc = (Vector2){.x = distY * dir.x / dir.y, .y = distY};
        }
        Vector2 new_rs = Vector2Add(rs, inc);
        #ifdef DEBUG
        // draw raycast on minimap
        if (new_rs.x > -1.0 && new_rs.x <= COLS && new_rs.y > -1.0 && new_rs.y <= ROWS && rs.x > -1.0 && rs.x <= COLS && rs.y > -1.0 && rs.y <= ROWS) {
            DrawLineEx(Vector2Scale(rs, MINIMAP_CELL_SCALE), Vector2Scale(new_rs, MINIMAP_CELL_SCALE), LINE_THICKNESS, BLUE);
            // DrawCircleV(Vector2Scale(rs, MINIMAP_CELL_SCALE), POINT_R, RED);
        }
        #endif
        rs = new_rs;
    }
}

void move_player(Player *p) {
    float dt = GetFrameTime();
    if (IsKeyDown(KEY_A)) {
        p->dir = Vector2Rotate(p->dir, -dt * PLAYER_ROTATION_SPEED);
    }
    if (IsKeyDown(KEY_D)) {
        p->dir = Vector2Rotate(p->dir, dt * PLAYER_ROTATION_SPEED);
    }
    if (IsKeyDown(KEY_W)) {
        p->pos = Vector2Add(p->pos, Vector2Scale(p->dir, dt * PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_S)) {
        p->pos = Vector2Add(p->pos, Vector2Scale(p->dir, -dt * PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_E)) {
        p->pos = Vector2Add(p->pos, Vector2Scale(Vector2Rotate(p->dir, PI / 2.0), dt * PLAYER_SPEED));
    }
    if (IsKeyDown(KEY_Q)) {
        p->pos = Vector2Add(p->pos, Vector2Scale(Vector2Rotate(p->dir, -PI / 2.0), dt * PLAYER_SPEED));
    }
}

void draw_walls(Player p) {
    float alpha = -FOV_ANGLE / 2.0;
    float alpha_step = FOV_ANGLE * RAY_RES / SCREEN_W;
    for (int slice_x = 0; slice_x < SCREEN_W; slice_x += RAY_RES) {
        Vector2 ray = Vector2Rotate(p.dir, alpha);
        raycast_walls(p, ray, slice_x);
        alpha += alpha_step;
    }
}

#ifdef ESP32
int app_main()
#else
int main()
#endif
{
    init_game();
    InitWindow(SCREEN_W, SCREEN_H, "ray");
    SetTargetFPS(TARGET_FPS);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    Player p = {.pos = {.x = 0.2, .y = 1.3}, .dir = {.x = 1, .y = 0}};

    while (!WindowShouldClose()) {
        move_player(&p);
        BeginDrawing();
        ClearBackground(BLACK);
        draw_walls(p);
        #ifdef DEBUG
        draw_minimap();
        draw_minimap_player(p.pos);
        #endif
        EndDrawing();
    }
    return 0;
}
