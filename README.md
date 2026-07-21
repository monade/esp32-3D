
# yari

**Yet Another Raycasting Implementation**

A small C game engine for building vintage first-person games in the spirit of
Wolfenstein 3D. YARI is focused on ESP32 hardware, but the same game code also
runs on macOS, Linux and WebAssembly through raylib or SDL2.


[![C](https://img.shields.io/badge/C-99-00599C?style=flat-square)](#) [![ESP32](https://img.shields.io/badge/ESP32-ST7789-E7352C?style=flat-square)](#supported-targets) [![raylib](https://img.shields.io/badge/raylib-5.5-111111?style=flat-square)](#desktop-macoslinux) [![SDL2](https://img.shields.io/badge/SDL2-2.x-173B6D?style=flat-square)](#desktop-macoslinux) [![WebAssembly](https://img.shields.io/badge/WebAssembly-Emscripten-654FF0?style=flat-square)](#webassembly)

**Try the [browser demo](https://monade.github.io/yari/#play)**

[![ESP32 video](docs/media/esp32_demo.webp)](#)

## Table of Contents

- [What is YARI?](#what-is-yari)
- [Quick Start](#quick-start)
- [Supported Targets](#supported-targets)
- [Project Layout](#project-layout)
- [Writing a Game](#writing-a-game)
- [Core API](#core-api)
- [Utils](#utils-yari_utilsh)
- [Maps, Assets and Fonts](#maps-assets-and-fonts)
- [Map Builder](#map-builder)
- [Build Commands](#build-commands)
- [ESP32 Configuration](#esp32-configuration)
- [Compatibility and Current Limits](#compatibility-and-current-limits)
- [Included Examples](#included-examples)
- [Acknowledgements](#acknowledgements)

## What is YARI?

`yari` is a compact raycasting engine for old-school 2.5D games.

It is not a general-purpose game engine. The goal is to stay small,
understandable and practical on constrained hardware, while keeping desktop
development fast enough to iterate without flashing an ESP32 after every
change.

The repository includes:

- the engine core in `src/yari`;
- renderer and input backends for ESP32, raylib and SDL2;
- desktop, web and ESP-IDF examples;
- tools that convert images and fonts into C headers;
- a visual level editor that generates YARI-compatible `level.h` files.

## Highlights

- **C raycasting renderer**: column-based wall rendering, sprite z-buffering,
  distance shading and floor/ceiling casting.
- **ESP32-first design**: ST7789 SPI backend, RGB565 framebuffer, configurable
  LCD pins and ready-to-build ESP-IDF examples.
- **Desktop iteration**: the same game can run on macOS/Linux through raylib or
  SDL2, which makes development and debugging much faster.
- **Web output**: the full game example can be compiled to WebAssembly and run in a browser.
- **Static assets**: textures and fonts are packed into C arrays, making them
  easy to ship inside embedded firmware.
- **Built-in map editor**: `map_builder` generates map data, player settings,
  surfaces, entities, collision layers and reloadable editor metadata.
- **Small game-facing API**: a game implements `yr_init_game()` and
  `yr_update_game()`. YARI owns the platform loop, rendering setup and input
  setup.

## Quick Start

### ESP32

Requirements:

- ESP-IDF installed and configured;
- a connected ESP32 board;
- an ST7789 display matching the default pin configuration, or custom LCD/pin
  macros supplied at build time.

The project has been developed with ESP-IDF 5.5.2.

```sh
make esp32-build
make esp32-flash-monitor
```

If ESP-IDF is not installed under the path used by the `Makefile`, pass
`ESP32_HOME` explicitly:

```sh
make ESP32_HOME="$HOME/esp/v5.5.2/esp-idf" esp32-build
```

### Desktop macOS/Linux

Requirements:

- a C compiler;
- `make`;
- `pkg-config`;
- raylib and/or SDL2 available through `pkg-config`.

For the default raylib backend on macOS with Homebrew:

```sh
brew install raylib
make run
```

For the SDL2 backend on macOS with Homebrew:

```sh
brew install sdl2
make run-sdl
```

On Linux, install the raylib or SDL2 development package with your
distribution's package manager, then run one of:

```sh
make run
make run-sdl
```

`make run` builds the raylib backend. `make run-sdl` builds the SDL2 backend.
Both start the complete example in `example/fps/main/main.c`.

### WebAssembly

Requirements:

- Emscripten active in the shell (`emcc` and `emar` in `PATH`);
- `npx` if you want to serve locally.

```sh
make wasm
make run-wasm
```

The web build writes `docs/index.html`, `docs/index.js` and `docs/index.wasm`.

## Supported Targets

| Target | Backend | Status |
| --- | --- | --- |
| ESP32 | ST7789 renderer + GPIO/ADC input | supported |
| macOS | raylib or SDL2 | supported |
| Linux | raylib or SDL2 | supported |
| WebAssembly | raylib PLATFORM_WEB + Emscripten | supported |
| Windows | raylib | supported |

## Project Layout

```text
.
├── example/                 # Example games
├── src/
│   ├── tools/               # assets_packer, font_baker, map_builder
│   └── yari/                # Engine source
│       ├── platform/esp32/  # ESP32 backend
│       ├── platform/raylib/ # Desktop/web backend
│       └── platform/sdl/    # Desktop SDL2 backend
└── Makefile                 # Desktop, WASM, ESP32 and tool builds
```

## Writing a Game

A YARI game includes `yari.h`, defines `YARI_MAIN` and implements two
functions:

```c
#define YARI_MAIN
// #define YARI_NO_PREFIX
#include <yari.h>

...

void yr_init_game(YrContext *ctx) {
    // configure the engine
    ctx->map = (YrMap){
        .walls = (YrWall *)walls,
        .cols = COLS,
        .rows = ROWS,
    };
    ctx->camera = (YrCamera){.pos = {14.5, 5.5}, .dir = {-0.8, 0.5}};
}

void yr_update_game(YrContext *ctx) {
    // update the game state and draw the frame
    yr_draw_game(ctx);
}
```

You can find a minimal example in `example/base/main/main.c`:


`YARI_NO_PREFIX` is optional. Without it, use the explicit `yr_` and `Yr`
symbols, such as `yr_draw_game()` and `YrContext`.

## Core API

Full function reference: [CHEATSHEET.md](CHEATSHEET.md).

### Engine context

`YrContext` is the central structure used by the engine.

| Field | Purpose |
| --- | --- |
| `camera` | camera position, direction, offset and rotation |
| `screen_width`, `screen_height` | framebuffer resolution |
| `game_title` | desktop window title |
| `target_fps` | target frame rate, zero for unlimited |
| `map` | tile map: a `YrMap` (`walls`, `cols`, `rows`, plus `floor_texture`/`ceil_texture` defaults and optional per-cell `floor`/`ceil` texture overrides - see Map Format) |
| `entities` | hash map of sprites/objects, keyed by a stable `size_t` id (see Entities) |
| `ray_res` | pixel width of each cast ray; larger values are faster but blockier |
| `zbuffer` | internal wall-depth buffer allocated by YARI |
| `assets_map` | texture lookup table generated by `assets_packer` |
| `game_data` | user-defined pointer for custom game state |

### Rendering


`yr_draw_game(ctx)` draws background, walls and entities.
The raycaster assumes `64x64` textures for walls, floors, ceilings and
entities (`YR_TEXTURE_SIZE`). HUD can be drawn with `yr_draw_texture(...)` and with the other drawing functions.


`yr_pixel_t` is the pixel format used by the framebuffer:

| Build | `yr_pixel_t` | Notes |
|-------|-------------|-------|
| `YR_RGB565` (ESP32 default) | `uint16_t` | RGB565 5-6-5 format |
| `YR_L8` (desktop/web/mono) | `uint8_t` | 8-bit grayscale luminance |
| default (desktop/web) | `uint32_t` | ARGB 8-8-8-8 format |

On ESP32, `YR_RGB565` is defined automatically unless `YR_L8` (or `YR_MONOCROME`) is set. Desktop/web default to ARGB32.

### Color filters

`YrColorFilterCallback` is a per-pixel color callback:

```c
typedef void (*YrColorFilterCallback)(int x, int y, yr_pixel_t *color, void *user_data);
```

**`yr_apply_color_filter(apply, user_data)`** touches every pixel exactly once, so its cost 
is fixed at `screen_width * screen_height` regardless of overdraw. 
Use it for full-screen effects (color grading, vignette, scanlines, day/night tinting):

```c
yr_draw_game(ctx);
yr_apply_color_filter(sepia_filter, NULL);
draw_hud(ctx); // drawn after, so the HUD stays unfiltered
```
The `apply` callback receives the pixel coordinates and a pointer to the color
value, which can be modified in-place. The `user_data` pointer is passed through unchanged and can be used to pass extra parameters to the callback.
The pixel format is `yr_pixel_t`, which is `uint16_t` in RGB565 builds and `uint32_t` in desktop/web builds, take care of that in your callback.

### Monochrome / L8 mode

Define `YR_L8` or `YR_MONOCROME` (which implies `YR_L8`) to use 8-bit grayscale pixels everywhere. Colors are stored as luminance (0–255). The pixel type is `uint8_t`.

`YR_MONOCROME` simulates on desktop the dithered 1bpp path used by the ESP32 backend for monochrome panels (e.g. SSD1306). The engine runs normally in L8; only the final blit to the display thresholds each pixel to black/white using a Bayer 4×4 ordered dithering matrix (`yr_bayer4x4`, accessible as `yr_mono_dither_lit(luma, x, y)`).

On ESP32 an `#elif` branch for `LCD_CONTROLLER_SSD1306` in `lcd_init()` selects the monochrome panel driver; the same Bayer dithering is applied in the blit path.

Regardless of the mode, keep `apply` in integer math on ESP32: the Xtensa FPU
is single-precision only, so `double` arithmetic (bare float literals like
`0.393`, `fmin`) is emulated in software and can drop frame rate from ~30 FPS
to single digits. `example/fps/main/main.c` has a fixed-point sepia filter
that budgets coefficients as integers scaled by 256:

```c
int r = (*color >> 11) & 0x1F, g = (*color >> 6) & 0x1F, b = *color & 0x1F;
int nr = (101 * r + 197 * g + 48 * b) >> 8;
if (nr > 31) nr = 31;
// ... same for g (>> 7, clamp 63) and b (>> 8, clamp 31)
```

### Physics and Collisions


All collision functions return or populate a `YrCollisionInfo`:

```c
typedef struct {
    YrCollisionType type;      // YR_COLLISION_NONE, YR_COLLISION_WALL, YR_COLLISION_ENTITY
    union {
        // valid only when type == YR_COLLISION_WALL
        struct {
            int cell_x, cell_y; // map cell of the hit wall
            YrWall tile;        // the hit wall itself (kind, texture_id/color, slide_x/y, ...)
        };
        // valid only when type == YR_COLLISION_ENTITY - shares memory with the struct above
        struct {
            YrEntity *entity;       // pointer to the hit entity
            size_t entity_index;    // id of the hit entity in ctx->entities
        };
    };
} YrCollisionInfo;
```

Built-in collision masks:

```c
#define YR_CMSK_NONE 0
#define YR_CMSK_WALL 1
#define YR_CMSK_ALL  -1
```

Entities can use custom bit masks for collision layers. The map builder can
define custom layers and generate macros such as `YR_CMSK_ENTITY`, `YR_CMSK_PLAYER`...

### Input

Map your esp32 key bindings in `yr_init_game(ctx)` using `yr_esp_key_init` and `yr_esp_joystick_init`

Key codes are defined in `src/yari/inputs.h` and follow raylib values. The SDL2
backend maps SDL key events into the same YARI key enum, so game code can be
shared across desktop backends and embedded targets.

### Entities

`YrEntity` represents a sprite in the world.

| Field | Purpose |
| --- | --- |
| `pos` | position in map space |
| `texture_id` | index inside `assets_map` |
| `kind` | user-defined entity kind id, set by the map builder (`YR_KIND_*`) or game code |
| `dist` | distance from the player, maintained by the renderer |
| `vdiv`, `hdiv` | vertical/horizontal sprite size reduction |
| `vmove` | perspective vertical offset |
| `disabled` | if `true`, the entity is skipped |
| `entity_data` | user-defined pointer |
| `collision_mask` | entity collision layer |
| `collision_threshold` | collision radius |
| `animation` | embedded `YrAnimationStack` |
| `init` | optional callback run once by `yr_create_entity_ex` when the entity is spawned: `void(YrEntity *self, void *data)` |
| `update` | optional callback invoked every frame: `void(YrContext *ctx, YrEntity *self, size_t id)` |
| `cleanup` | optional callback run by `yr_remove_entity`: `void(YrEntity *self)`; use it to free `entity_data` |

`ctx->entities` is a `YrEntityMap` (a `yr_Hm(size_t, YrEntity)` hash map - see the Hash Map and Hash Set section under Utils), not a plain array. Entities are addressed by a stable id that keeps working across other insertions/removals, so it's safe to remove one while iterating or holding onto its id across frames:

```c
size_t id = yr_create_entity(state, entity);       // inserts entity, returns its new id
size_t id2 = yr_create_entity_ex(state, entity, p); // same, and passes p to entity.init (if set)
yr_remove_entity(state, id);                        // runs entity->cleanup (if set), then removes it
```

The `id`/`index` argument passed to `update` callbacks, and `YrCollisionInfo.entity_index`, are this same persistent id. Use it with `yr_remove_entity` or `yr_hm_try(&ctx->entities, id)` - it is not an array offset. Given a live `YrEntity *`, `yr_get_entity_id(e)` recovers its id.

To iterate all entities:

```c
yr_foreach(&ctx->entities, kv) {
    YrEntity *e = &kv->value; // kv->key is the entity id
}
```

## Utils

### Timers

`YrTimer` is a lightweight countdown timer. See [CHEATSHEET.md](CHEATSHEET.md) for the full function list.

The `count` field on `YrTimer` increments each time `yr_timer_loop` fires.

### Sprite Animation

`YrAnimationStack` manages a stack of animations. An animation is described by a `YrAnimation` value (`frames`, `frame_count`, `duration`); push one with the helpers in [CHEATSHEET.md](CHEATSHEET.md). Popping the top animation (once its lifetime expires) resumes the one beneath it.

Every `YrEntity` has its own `animation` stack, and `yr_draw_entities` advances each entity's stack and writes the result into `texture_id` automatically every frame - game code only ever pushes animations, it never needs to call an "advance" function for entities.

### Layout Helpers

Designate HUD/UI elements in normalized (`-1..1`) or absolute screen coordinates, with alignment and an optional bounding box, through variadic macros:

```c
// Screen coordinate conversions
Vector2 yr_screen_coord(Vector2 pos);        // normalized -1..1 → screen pixels from center origin
Vector2 yr_screen_coord_abs(Vector2 pos);    // normalized 0..1 → screen pixels


// Layout-aware text and texture drawing (variadic struct params)
yr_draw_text_ex(txt, font, .color=YR_WHITE, .align=YR_LAY_CENTER, ...);
yr_draw_texture_ex(texture, txw, txh, .align=YR_LAY_CENTER, .box={200, 100}, ...);
```

Alignment values (`enum lay_pos`): `YR_LAY_NONE`, `YR_LAY_CENTER`, `YR_LAY_CB` (center-bottom), `YR_LAY_CT` (center-top), `YR_LAY_CL` (center-left), `YR_LAY_CR` (center-right), `YR_LAY_TL` (top-left), `YR_LAY_TR`, `YR_LAY_BR`, `YR_LAY_BL`.

Display modes (`enum lay_dis`): `YR_LAY_SCREEN` (position as absolute pixel x, y), `YR_LAY_NORM` (position as normalized 0..1 nx, ny).

The struct params accept `.color`, `.align`, `.display`, `.box` (bounding box), and `.length`/`.draw_empty` as needed. See CHEATSHEET.md for all fields.

Example - looping idle with a one-shot attack that auto-pops, driven from an entity's `init`/`update` callbacks:

```c
// the map builder can generate these as named YrAnimation constants instead
static const int idle_frames[]   = {tx_idle0, tx_idle1, tx_idle2};
static const int attack_frames[] = {tx_atk0, tx_atk1, tx_atk2, tx_atk3};

YrAnimation idle_anim   = {.frames = idle_frames, .frame_count = 3, .duration = 0.15f};
YrAnimation attack_anim = {.frames = attack_frames, .frame_count = 4, .duration = 0.1f};

void init_enemy(YrEntity *self, void *data) {
    (void)data;
    yr_start_loop_animation(&self->animation, idle_anim); // push idle on spawn
}

void update_enemy(YrContext *ctx, YrEntity *self, size_t id) {
    (void)state;
    if (/* attack triggered */ false) {
        // push attack on trigger - auto-pops, idle resumes underneath
        yr_start_animation_once(&self->animation, attack_anim);
    }
}
```

For an animation not tied to an entity (a HUD weapon sprite, say), call `yr_get_animation_texture(&stack)` yourself each frame to read the current frame's texture id (or `-1` if the stack is empty).

## Maps, Assets and Fonts

### Map Format

`ctx->map` is a `YrMap`: a `walls` array of `rows * cols` `YrWall` cells, plus
`floor_texture`/`ceil_texture` (default floor/ceiling texture ids, `0` for
black) and optional per-cell `floor`/`ceil` `uint8_t` arrays that override the
default texture for individual cells (`0` means "use the default").

| Attribute | Meaning |
| --- | --- |
| kind | YR_WK_EMPTY (empty cell), YR_WK_FULL, ... |
| textured | if false color will be applied instead of texture_id |
| texture_id | texture id, indexed through `assets_map` |
| color | solid color |

Player and entity coordinates are floating-point values in the same map space.
Cell `(x, y)` covers the area `[x, x+1)`, `[y, y+1)`.

### Image Assets

Source assets are converted into static C arrays by `assets_packer`.

```sh
make assets
# it runs build/yari/bin/assets_packer example/fps/assets example/fps/main/assets.h
```

`assets_packer`:

- reads `.png` and `.jpg` files from the directory passed as the first argument;
- generates one `yr_pixel_t` array per image;
- generates a `TextureId` enum with `tx_<file_name>` symbols;
- generates `assets_map[]`;
- emits both RGB565 data for `YR_RGB565` builds and 32-bit data for
  desktop/web builds.
- the output file is written to the path passed as the second argument.

Use simple C-friendly file names, for example `wal_001.png`, `wep_gun0.png` or
`door_metal.png`.

### Fonts

`.ttf` files under `assets/font/` are baked into
`example/fps/main/fonts.h`:

```sh
make assets
# it runs build/yari/bin/font_baker example/fps/assets/font example/fps/main/fonts.h
```

- reads `.ttf` files from the directory passed as the first argument;
- the output file is written to the path passed as the second argument;

`font_baker` generates four font sizes:

- `YR_FONT_SM`;
- `YR_FONT_MD`;
- `YR_FONT_LG`;
- `YR_FONT_XL`.

`yr_get_text_length(text, len, font)` measures the pixel width of `len` characters in the given font (pass `SIZE_MAX` or `0` to measure the full null-terminated string).

Example:

```c
yr_draw_text("HP: 100", 10, 15, fonts[YR_FONT_SM], YR_GREEN);
float w = yr_get_text_length("HP: 100", 0, fonts[YR_FONT_SM]); // pixel width
```

## Map Builder

[![map builder](docs/media/map_builder.png)](#)

YARI includes a raylib/raygui visual level editor:

```sh
make edit-fps
make edit-kart
```

To build the editor without launching it:

```sh
make map-builder
```

`make edit-fps` builds the tool and runs:

```sh
build/yari/bin/map_builder assets example/fps/main/level1.h
```

The executable accepts optional paths:

```sh
build/yari/bin/map_builder [assets_dir] [output_file]
```

If omitted, `assets_dir` defaults to `assets` and `output_file` defaults to
`level.h`. The asset directory is scanned for `.png`, `.jpg` and `.jpeg` files;
their names are converted to the same `tx_<file_name>` symbols generated by
`assets_packer`, so run `make assets` after adding or renaming textures.

On startup, the editor tries to load the `MAP_BUILDER_STATE_BEGIN/END` metadata
from the output files. Press `Save` to overwrite the output header and `Load` to
reload the last saved state.

It generate a `level_gen.h` that contains:
- custom collision layers;
- entity kinds (`YR_KIND_*`);
- init/update/cleanup callback forward declarations;
- animations as named `YrAnimation` constants (frames, frame count and duration);

And a level file contains:
- map dimensions (`YR_MAP_COLS`, `YR_MAP_ROWS`);
- wall grid data;
- floor and ceiling texture ids;
- player start position;
- inline factories for entities (the generated factories also starts the idle animation if setted and setup configured callbacks);
- `level_append_exported_entities()`; // appends entities marked as `exported` in the editor to the game state
- `level_get_map()`;

Include the generated level file header from game code and wire it into `yr_init_game()`:

```c
#include "assets.h" // generated by assets_packer
#include "level.h" // generated by map_builder

void yr_init_game(Context *ctx) {
    load_level(ctx);
    // ...
}
```

Entities marked as `exported` in the editor are appended in the game state entities.
If you want to spawn entities at runtime you can remove the `exported` flag and call the factory functions directly, for example:

```c
YrEntity enemy = create_enemy_pos((Vector2){10.0f, 5.0f}, NULL, init_enemy, update_enemy, cleanup_enemy);
yr_create_entity(state, enemy);
```

A factory only takes an explicit `init`/`update`/`cleanup` parameter (in that order) when the entity has no fixed callback of that kind assigned in the editor; when one is assigned, it's baked into the factory and the corresponding parameter disappears from its signature. e.g. an entity with all three assigned generates `create_enemy_pos(pos, data)`, taking no callback parameters at all.

Entities with a named update callback generate a forward declaration for that function, so implement it in game code
with this signature:

```c
void update_enemy(YrContext *ctx, YrEntity *self, size_t id) {
    (void)state;
    (void)self;
    (void)id;
}
```

Entities can also be assigned `init` and `cleanup` callbacks (set in the editor's `Functions` tab, next to `update`). Both also generate forward declarations. `init` runs once when the entity is spawned via `yr_create_entity`/`yr_create_entity_ex`, typically to `calloc` and populate `entity_data`; `cleanup` runs once when the entity is removed via `yr_remove_entity`, typically to free it:

```c
void init_enemy(YrEntity *self, void *data) {
    (void)data;
    self->entity_data = calloc(1, sizeof(EnemyData));
}

void cleanup_enemy(YrEntity *self) {
    free(self->entity_data);
}
```

Useful map builder controls:

| Action | Control |
| --- | --- |
| save | `Save` button or `Ctrl/Cmd+S` |
| reload level | `Load` button |
| fit view | `Fit` button |
| copy selection | `Ctrl/Cmd+C` |
| paste selection | `Ctrl/Cmd+V` |
| delete selection | `Delete` or `Backspace` |
| pan | middle mouse button, or `Space` + left drag |
| zoom | mouse wheel with `Ctrl/Cmd` |

Main editor modes:

- `Wall`: draw walls as points, rectangles or circles.
- `Entity`: place sprites with texture, collision mask and kind on the `Properties` tab, init/update/cleanup callbacks on the `Functions` tab.
- `Player`: edit player position, direction, collision radius and collision layers.
- `Floor/Ceil`: assign floor and ceiling textures.
- `Anim`: create and edit animations for entities, including frame list, duration.

## Build Commands

| Command | Effect |
| --- | --- |
| `make run` | builds and runs `example/fps` with raylib on desktop |
| `make run-sdl` | builds and runs `example/fps` with SDL2 on desktop |
| `make run-base` | builds and runs the minimal example |
| `make assets` | regenerates `assets.h` and `fonts.h` |
| `make edit-fps` | builds and runs the map editor for `example/fps` |
| `make edit-kart` | builds and runs the map editor for `example/kart` |
| `make run-wasm` | builds and serves the WebAssembly `example/fps` |
| `make esp32-build` | builds `example/fps` with ESP-IDF |
| `make esp32-flash` | builds and flashes `example/fps` |
| `make esp32-monitor` | opens the serial monitor |
| `make esp32-flash-monitor` | flashes and opens the serial monitor |
| `make esp32-clean` | runs `idf.py fullclean` in `example/fps` |
| `make esp32-base-build` | builds `example/base` for ESP32 |
| `make esp32-base-flash-monitor` | flashes and monitors the base example |

`make all` builds desktop, WebAssembly and ESP32 targets. For day-to-day work,
use narrower targets such as `make run` or `make esp32-flash`.

## ESP32 Configuration

The ESP32 backend is implemented in `src/yari/platform/esp32/renderer.c` on
top of ESP-IDF's `esp_lcd` component, over SPI. It defaults to an ST7789
panel in landscape orientation (the T-Display's display), but every pin,
bus parameter and panel trait is a `#define` so it can be reconfigured, or
retargeted to a different controller, from your build without touching the
file.

Main configuration macros:

```c
// Framebuffer / active area
#define YR_LCD_W 240
#define YR_LCD_H 136
#define LCD_X_OFF 40   // gap between GRAM origin and visible glass
#define LCD_Y_OFF 53

// SPI pins and bus
#define PIN_MOSI 19
#define PIN_CLK 18
#define PIN_CS 5
#define PIN_DC 16
#define PIN_RST 23      // set to -1 if the panel has no reset line
#define PIN_BL 4        // set to -1 if the backlight isn't GPIO-controlled
#define LCD_BL_ACTIVE_LOW false
#define LCD_SPI_HOST SPI2_HOST
#define LCD_SPI_MODE 0
#define SPI_CLOCK_SPEED (80 * 1000 * 1000)

// Panel controller: define one to select it (defaults to LCD_CONTROLLER_ST7789)
#define LCD_CONTROLLER_ST7789
// #define LCD_CONTROLLER_SSD1306

// Rotation / color, applied through esp_lcd's portable panel ops
#define LCD_MIRROR_X true
#define LCD_MIRROR_Y false
#define LCD_SWAP_XY true
#define LCD_BGR_ORDER false
#define LCD_INVERT_COLOR true   // defaults to false for non-ST7789 controllers

// Color depth: 1 = monochrome (packed per the controller's native GDDRAM
// layout), 16 = RGB565 (fast path, zero-copy blit), anything else is
// treated as N bytes/pixel and produced by expanding RGB565 per channel.
#define LCD_BITS_PER_PIXEL 16
```

`esp_lcd` ships ST7789 and SSD1306 (monochrome) drivers with no extra
dependencies; both are wired up above. To target another `esp_lcd`-compatible
controller (e.g. ILI9341, GC9A01, either built into `esp_lcd` or added as a
managed component via `idf_component.yml`), add an `#elif` branch next to
`esp_lcd_new_panel_st7789(...)` in `lcd_init()` calling its own
`esp_lcd_new_panel_xxx()` constructor — the SPI bus setup, framebuffer,
rotation and blit/pack code are all controller-agnostic already.

`ESP32` is defined automatically by the `yari` component itself. The pixel
format is your choice, so the ESP-IDF examples add it explicitly:

```cmake
idf_build_set_property(COMPILE_OPTIONS "-DYR_RGB565" APPEND)
```

To use YARI as an ESP-IDF component in another project, add it as a
dependency in your `main/idf_component.yml`:

```yaml
dependencies:
  idf: ">=4.1.0"
  yari:
    git: "https://github.com/monade/esp32-3D.git"
    path: "src/yari"
    version: "main"   # pin to a tag or commit for reproducible builds
```
### SDK configs

For optimal performance you should keep the esp32 cpu freq to max 240mhz, use compiler optimization flags and define `-DYR_MULTITHREAD`, check examples `CMakeList.txt` and `sdkconfig`.
If you need more space for assets you should define a custom partition table according to your device flash storage capability using `idf.py menuconfig`.

### ESP32 Input

Digital buttons are configured as pull-up GPIOs:

```c
yr_esp_key_init(25, YR_KEY_Q);
yr_esp_key_init(2, YR_KEY_E);
yr_esp_key_init(15, YR_KEY_X);
yr_esp_key_init(26, YR_KEY_SPACE);
```

The analog joystick uses two ADC pins:

```c
int joystick_id = yr_joystick_init(32, 36);
float x = yr_joystick_get_axis(joystick_id, YR_X_AXIS);
float y = yr_joystick_get_axis(joystick_id, YR_Y_AXIS);
```

### ESP32 Example diagram
[![diagram](docs/media/esp32_diagram.png)](#)
[Wokwi diagram](https://wokwi.com/projects/468287699711870977)

## Compatibility and Current Limits

- The project is plain C.
- The ESP32 renderer currently targets ST7789 SPI displays with an RGB565 framebuffer.
- Desktop rendering can use raylib or SDL2; web rendering uses raylib.
- Raycaster textures are expected to be `64x64`.
- Map cells are `YrWall` structs (`kind`, `textured`, `texture_id`/`color`,
  `slide_x`/`slide_y`), not raw byte values - see Map Format.
- Desktop joystick backends are currently stubs.

## Included Examples

### `example/base`

A minimal example with an in-memory map and solid-color walls. Use it to learn
the engine contract without the asset pipeline.

```sh
make run-base
```

### `example/fps` and `example/kart`

Two complete examples with:

- packed assets from `assets/`;
- bitmap fonts;
- a level generated by the map builder;
- player movement;
- HUD rendering;
- a weapon pickup;
- desktop, ESP32 and WebAssembly builds.


```sh
# fps
make esp32-flash
make run
make run-sdl
make run-wasm

# kart
make esp32-kart-flash
make run-kart
make run-sdl-kart

```

## Acknowledgements

- [lodev](https://lodev.org/cgtutor/raycasting.html) raycasting tutorial
- [raylib](https://github.com/raysan5/raylib) used for the desktop and web backends