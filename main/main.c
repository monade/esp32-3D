#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "raylib.h"
#include "raymath.h"

#define ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

#ifdef ESP32
    #define TARGET_FPS 30
    #define SCREEN_W LCD_W
    #define SCREEN_H LCD_H
    #define RAY_RES 0.008
    #define PLAYER_ROTATION_SPEED 1.25
#else
    #define DEBUG 1
    #define TARGET_FPS 60
    #define SCREEN_W 800
    #define SCREEN_H 600
    #define RAY_RES 0.005
    #define PLAYER_ROTATION_SPEED 2.0
#endif
#define COLS 10
#define ROWS 10
#define ASPECT_RATIO ((float)SCREEN_W / SCREEN_H)
#define MINIMAP_CELL_RES 25
#define MINIMAP_W (COLS * MINIMAP_CELL_RES)
#define MINIMAP_H (ROWS * MINIMAP_CELL_RES)
#define FOV_ANGLE (PI / 3.5)
#define PLAYER_SPEED 100.0
#define MAX_RENDER_DIST MINIMAP_CELL_RES * 20.0

#define POINT_R 2.5
#define LINE_THICKNESS 1.5

typedef enum {
    CELL_COLOR,
    CELL_TEXTURE,
} GameCellType;

typedef struct {
    GameCellType type;
    union {
        Color color;
        // TODO: texture
    };
} GameCell;

typedef struct {
    Vector2 pos;
    Vector2 dir;
} Player;

static GameCell wallr = {.type = CELL_COLOR, .color = RED};
static GameCell wally = {.type = CELL_COLOR, .color = YELLOW};
static GameCell wallg = {.type = CELL_COLOR, .color = GREEN};

static GameCell *map[ROWS][COLS] = {0};

void init_game() {
    map[1][3] = &wallr;
    map[1][4] = &wally;
    map[1][5] = &wallg;
    map[2][5] = &wallr;
    map[3][4] = &wallg;
    map[3][5] = &wallr;
}

void draw_minimap() {
    DrawRectangleLines(0, 0, MINIMAP_W, MINIMAP_H, RAYWHITE);
    for (int i = 1; i < COLS; i++) {
        int x = i * MINIMAP_CELL_RES;
        DrawLine(x, 0, x, MINIMAP_H, RAYWHITE);
    }
    for (int i = 1; i < ROWS; i++) {
        int y = i * MINIMAP_CELL_RES;
        DrawLine(0, y, MINIMAP_W, y, RAYWHITE);
    }
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            GameCell *c = map[i][j];
            if (c) {
                DrawRectangle(j * MINIMAP_CELL_RES, i * MINIMAP_CELL_RES, MINIMAP_CELL_RES, MINIMAP_CELL_RES, c->color);
            }
        }
    }
}

void draw_minimap_player(Vector2 p) {
    DrawCircleV(p, POINT_R * 2.0, GREEN);
}

void cast_ray(Player p, Vector2 dir, int slice_x, float slice_w) {
    if (dir.x == 0.0) dir.x = EPSILON;
    if (dir.y == 0.0) dir.y = EPSILON;
    Vector2 rs = Vector2Add(p.pos, dir);
    while (Vector2Length(rs) <= MAX_RENDER_DIST) {
        Vector2 cell = {.x = floorf(rs.x / MINIMAP_CELL_RES), .y = floorf(rs.y / MINIMAP_CELL_RES)};
        if (rs.x > 0.0 && rs.x < MINIMAP_W && rs.y > 0.0 && rs.y < MINIMAP_H) {
            GameCell *map_cell = map[(int)cell.y][(int)cell.x];
            if (map_cell) {
                // draw slice
                float dist = Vector2DotProduct(Vector2Subtract(rs, p.pos), p.dir) / (MINIMAP_CELL_RES * ASPECT_RATIO);
                float h = SCREEN_H / dist;
                float bright_factor = 1.0 / dist - 0.75;
                if (bright_factor >= 0.0) bright_factor = 0.0;
                Color c = ColorBrightness(map_cell->color, bright_factor);
                DrawRectangle(slice_x, (SCREEN_H - h) / 2.0, slice_w, h, c);
                return;
            }
        }
        float distX = MINIMAP_CELL_RES * (cell.x + (dir.x >= 0 ? 1.0 : -EPSILON)) - rs.x;
        float distY = MINIMAP_CELL_RES * (cell.y + (dir.y >= 0 ? 1.0 : -EPSILON)) - rs.y;
        Vector2 inc;
        if (fabs(distX / dir.x) < fabs(distY / dir.y)) {
            inc = (Vector2){.x = distX, .y = distX * dir.y / dir.x};
        } else {
            inc = (Vector2){.x = distY * dir.x / dir.y, .y = distY};
        }
        rs = Vector2Add(rs, inc);
        // draw raycast
        if (rs.x > -1.0 && rs.x <= MINIMAP_W && rs.y > -1.0 && rs.y <= MINIMAP_H) {
            DrawLineEx(p.pos, rs, LINE_THICKNESS, BLUE);
            // DrawCircleV(rs, POINT_R, RED);
        }
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
    Player p = {.pos = {.x = 2.5, .y = 1.3}, .dir = {.x = 1, .y = 0}};

    while (!WindowShouldClose()) {
        move_player(&p);
        BeginDrawing();
        ClearBackground(BLACK);
        float slice_w = SCREEN_W / (FOV_ANGLE / RAY_RES);
        float alpha = -FOV_ANGLE / 2.0;
        for (int slice_x = 0; slice_x < SCREEN_W; slice_x += slice_w) {
            Vector2 ray = Vector2Rotate(p.dir, alpha);
            cast_ray(p, ray, slice_x, slice_w);
            alpha += RAY_RES;
        }
        #ifdef DEBUG
        draw_minimap();
        draw_minimap_player(p.pos);
        #endif
        EndDrawing();
    }
    return 0;
}
