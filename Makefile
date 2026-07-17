YARI_CFLAGS = -Wall -Wextra -O3
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib) -lm
SDL2_CFLAGS = $(shell pkg-config --cflags sdl2)
SDL2_LIBS = $(shell pkg-config --libs sdl2) -lm
RAYLIB_WEB_CFLAGS = -Wall -Wextra -O3 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -Iexternal/web/raylib/include -Ibuild/yari/include
RAYLIB_WEB_LDFLAGS = -s USE_GLFW=3 -s ASSERTIONS=1 -s INITIAL_MEMORY=33554432 --shell-file src/yari/platform/raylib/shell.html

MAIN_CFLAGS = -Wall -Wextra -O3 -Ibuild/yari/include
MAIN_LIBS = -Lbuild/yari/lib -lyari_raylib
SDL_MAIN_LIBS = -Lbuild/yari/lib -lyari_sdl

EMCC = emcc
EMAR = emar

WINCC=x86_64-w64-mingw32-gcc
WIN_CFLAGS=-Wall -Wextra -O2 -static -Isrc/yari -Iexternal/win/raylib/include
WIN_LIBS=-Lexternal/win/raylib/lib/ -l:libraylib.a -lopengl32 -lgdi32 -lwinmm

ESP32_HOME = ~/esp/v5.5.*/esp-idf

$(shell mkdir -p build)

.PHONY: all

all: build/yari/lib/libyari_raylib.a build/yari/lib/libyari_sdl.a build/yari/lib/libyari_web.a build/yari/esp32 build/yari/bin/assets_packer build/yari/bin/font_baker build/yari/bin/map_builder

# Lib yari

build/yari: src/yari/*.h
	rm -rf build/yari/include
	@mkdir -p build/yari/lib
	@mkdir -p build/yari/include
	@mkdir -p build/yari/bin
	cp src/yari/*.h build/yari/include/

## Raylib
build/yari.o: src/yari/yari.c
	$(CC) $(YARI_CFLAGS) -c src/yari/yari.c -o build/yari.o
build/renderer_common.o: src/yari/renderer_common.c
	$(CC) $(YARI_CFLAGS) -c src/yari/renderer_common.c -o build/renderer_common.o
build/physics.o: src/yari/physics.c
	$(CC) $(YARI_CFLAGS) -c src/yari/physics.c -o build/physics.o
build/yari_utils.o: src/yari/yari_utils.c
	$(CC) $(YARI_CFLAGS) -c src/yari/yari_utils.c -o build/yari_utils.o
build/ht.o: src/yari/ht.c
	$(CC) $(YARI_CFLAGS) -c src/yari/ht.c -o build/ht.o

build/inputs.o: src/yari/platform/raylib/inputs.c
	$(CC) $(YARI_CFLAGS) $(RAYLIB_CFLAGS) -c src/yari/platform/raylib/inputs.c -o build/inputs.o
build/renderer.o: src/yari/platform/raylib/renderer.c
	$(CC) $(YARI_CFLAGS) $(RAYLIB_CFLAGS) -c src/yari/platform/raylib/renderer.c -o build/renderer.o
build/yari/lib/libyari_raylib.a: build/yari build/yari.o build/renderer_common.o build/inputs.o build/renderer.o build/physics.o build/yari_utils.o build/ht.o
	ar rcs build/yari/lib/libyari_raylib.a build/yari.o build/renderer_common.o build/inputs.o build/renderer.o build/physics.o build/yari_utils.o build/ht.o

## SDL2
build/inputs_sdl.o: src/yari/platform/sdl/inputs.c src/yari/inputs.h
	$(CC) $(YARI_CFLAGS) $(SDL2_CFLAGS) -c src/yari/platform/sdl/inputs.c -o build/inputs_sdl.o
build/renderer_sdl.o: src/yari/platform/sdl/renderer.c src/yari/renderer.h
	$(CC) $(YARI_CFLAGS) $(SDL2_CFLAGS) -c src/yari/platform/sdl/renderer.c -o build/renderer_sdl.o
build/yari/lib/libyari_sdl.a: build/yari build/yari.o build/renderer_common.o build/inputs_sdl.o build/renderer_sdl.o build/physics.o build/yari_utils.o build/ht.o
	ar rcs build/yari/lib/libyari_sdl.a build/yari.o build/renderer_common.o build/inputs_sdl.o build/renderer_sdl.o build/physics.o build/yari_utils.o build/ht.o

## Raylib web
external/web:
	@if [ ! -d "external/web/raylib" ]; then \
		mkdir -p external/web; \
		curl -L https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_webassembly.zip -o external/web/raylib.zip; \
		unzip external/web/raylib.zip -d external/web; \
		mv external/web/raylib-5.5_webassembly external/web/raylib; \
		rm external/web/raylib.zip; \
	fi
build/yari_web.o: src/yari/yari.c src/yari/yari.h src/yari/renderer.h src/yari/inputs.h src/yari/colors.h src/yari/physics.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/yari.c -o build/yari_web.o
build/renderer_common_web.o: src/yari/renderer_common.c src/yari/renderer.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/renderer_common.c -o build/renderer_common_web.o
build/physics_web.o: src/yari/physics.c src/yari/physics.h src/yari/yari.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/physics.c -o build/physics_web.o
build/yari_utils_web.o: src/yari/yari_utils.c src/yari/yari_utils.h src/yari/yari.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/yari_utils.c -o build/yari_utils_web.o
build/ht_web.o: src/yari/ht.c src/yari/ht.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/ht.c -o build/ht_web.o
build/renderer_web.o: external/web src/yari/platform/raylib/renderer.c src/yari/renderer.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/platform/raylib/renderer.c -o build/renderer_web.o
build/inputs_web.o: external/web src/yari/platform/raylib/inputs.c src/yari/inputs.h
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -c src/yari/platform/raylib/inputs.c -o build/inputs_web.o
build/yari/lib/libyari_web.a: external/web build/yari build/yari_web.o build/renderer_common_web.o build/inputs_web.o build/renderer_web.o build/physics_web.o build/yari_utils_web.o build/ht_web.o
	$(EMAR) rcs build/yari/lib/libyari_web.a build/yari_web.o build/renderer_common_web.o build/inputs_web.o build/renderer_web.o build/physics_web.o build/yari_utils_web.o build/ht_web.o

## ESP32
build/yari/esp32: src/yari/* src/yari/platform/esp32/*
	rm -rf build/yari/esp32
	@mkdir -p build/yari/esp32
	cp -r src/yari build/yari/esp32

# Tools
.PHONY: assets map-builder run-map-builder

build/yari/bin/assets_packer: build/yari src/tools/assets_packer.c
	$(CC) -o build/yari/bin/assets_packer src/tools/assets_packer.c -lm

build/yari/bin/font_baker: build/yari src/tools/font_baker.c
	$(CC) -o build/yari/bin/font_baker src/tools/font_baker.c -lm

assets: build/yari/bin/assets_packer build/yari/bin/font_baker
	build/yari/bin/assets_packer example/fps/assets example/fps/main/assets.h
	build/yari/bin/font_baker example/fps/assets/font example/fps/main/fonts.h
	build/yari/bin/assets_packer example/kart/assets example/kart/main/assets.h
	build/yari/bin/font_baker example/kart/assets/font example/kart/main/fonts.h

build/yari/bin/map_builder: src/tools/map_builder.c src/tools/raygui.h
	$(CC) -Wall -Wextra -O2 $(RAYLIB_CFLAGS) -I./src/tools -o build/yari/bin/map_builder src/tools/map_builder.c $(RAYLIB_LIBS)
map-builder: build/yari/bin/map_builder
edit-fps: map-builder
	build/yari/bin/map_builder example/fps/assets example/fps/main/level1.h
edit-fps2: map-builder
	build/yari/bin/map_builder example/fps/assets example/fps/main/level2.h
edit-kart: map-builder
	build/yari/bin/map_builder example/kart/assets example/kart/main/level.h

# Examples

## Raylib examples

## Raylib examples

### base example
build/ray-base: build/yari/lib/libyari_raylib.a example/base/main/main.c
	$(CC) $(MAIN_CFLAGS) -o build/ray-base example/base/main/main.c $(MAIN_LIBS) $(RAYLIB_LIBS)

.PHONY: ray-base run-base
ray-base: build/ray-base
run-base: ray-base
	build/ray-base

### fps example
build/fps-raylib: assets build/yari/lib/libyari_raylib.a example/fps/main/main.c
	$(CC) $(MAIN_CFLAGS) -o build/fps-raylib example/fps/main/main.c $(MAIN_LIBS) $(RAYLIB_LIBS)

.PHONY: raylib run
fps-raylib: build/fps-raylib
run: fps-raylib
	build/fps-raylib

### SDL2 fps example
build/fps-sdl: assets build/yari/lib/libyari_sdl.a example/fps/main/main.c
	$(CC) $(MAIN_CFLAGS) -o build/fps-sdl example/fps/main/main.c $(SDL_MAIN_LIBS) $(SDL2_LIBS)

.PHONY: sdl run-sdl
sdl: build/fps-sdl
run-sdl: sdl
	build/fps-sdl

### WASM raylib example

docs/fps/index.html: build/yari/lib/libyari_web.a example/fps/main/main.c
	@mkdir -p docs/fps
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -o docs/fps/index.html \
		example/fps/main/main.c \
		build/yari/lib/libyari_web.a external/web/raylib/lib/libraylib.a $(RAYLIB_WEB_LDFLAGS) -lm

### kart example
build/kart-raylib: assets build/yari/lib/libyari_raylib.a example/kart/main/main.c
	$(CC) $(MAIN_CFLAGS) -o build/kart-raylib example/kart/main/main.c $(MAIN_LIBS) $(RAYLIB_LIBS)

.PHONY: kart-raylib run-kart
kart-raylib: build/kart-raylib
run-kart: kart-raylib
	build/kart-raylib

### SDL2 kart example
build/kart-sdl: assets build/yari/lib/libyari_sdl.a example/kart/main/main.c
	$(CC) $(MAIN_CFLAGS) -o build/kart-sdl example/kart/main/main.c $(SDL_MAIN_LIBS) $(SDL2_LIBS)

.PHONY: kart-sdl run-kart-sdl
kart-sdl: build/kart-sdl
run-kart-sdl: kart-sdl
	build/kart-sdl

### WASM kart example
docs/kart/index.html: build/yari/lib/libyari_web.a example/kart/main/main.c
	@mkdir -p docs/kart
	$(EMCC) $(RAYLIB_WEB_CFLAGS) -o docs/kart/index.html \
		example/kart/main/main.c \
		build/yari/lib/libyari_web.a external/web/raylib/lib/libraylib.a $(RAYLIB_WEB_LDFLAGS) -lm

.PHONY: wasm run-wasm
wasm: docs/fps/index.html docs/kart/index.html
run-wasm: wasm
	npx serve docs

## ESP32 examples

### base example
.PHONY: esp32-base-build esp32-base-flash esp32-base-monitor esp32-base-flash-monitor esp32-base-clean
esp32-base-build: build/yari/esp32
	cd example/base && . $(ESP32_HOME)/export.sh && idf.py build && idf.py size

esp32-base-flash: esp32-base-build
	cd example/base && . $(ESP32_HOME)/export.sh && idf.py flash

esp32-base-monitor:
	cd example/base && . $(ESP32_HOME)/export.sh && idf.py monitor

esp32-base-flash-monitor: esp32-base-flash
	cd example/base && . $(ESP32_HOME)/export.sh && idf.py monitor

esp32-base-clean:
	cd example/base && . $(ESP32_HOME)/export.sh && idf.py fullclean

### game example
.PHONY: esp32-build esp32-flash esp32-monitor esp32-flash-monitor esp32-clean
esp32-build: assets build/yari/esp32
	cd example/fps && . $(ESP32_HOME)/export.sh && idf.py build && idf.py size

esp32-flash: esp32-build
	cd example/fps && . $(ESP32_HOME)/export.sh && idf.py flash

esp32-monitor:
	cd example/fps && . $(ESP32_HOME)/export.sh && idf.py monitor

esp32-flash-monitor: esp32-flash
	cd example/fps && . $(ESP32_HOME)/export.sh && idf.py monitor

esp32-clean:
	cd example/fps && . $(ESP32_HOME)/export.sh && idf.py fullclean

### kart example
.PHONY: esp32-kart-build esp32-kart-flash esp32-kart-monitor esp32-kart-flash-monitor esp32-kart-clean
esp32-kart-build: assets build/yari/esp32
	cd example/kart && . $(ESP32_HOME)/export.sh && idf.py build && idf.py size

esp32-kart-flash: esp32-kart-build
	cd example/kart && . $(ESP32_HOME)/export.sh && idf.py flash

esp32-kart-monitor:
	cd example/kart && . $(ESP32_HOME)/export.sh && idf.py monitor

esp32-kart-flash-monitor: esp32-kart-flash
	cd example/kart && . $(ESP32_HOME)/export.sh && idf.py monitor

esp32-kart-clean:
	cd example/kart && . $(ESP32_HOME)/export.sh && idf.py fullclean

### Windows

external/win:
	@if [ ! -d "external/win" ]; then \
		echo "Downloading raylib for Windows..."; \
		mkdir -p external/win; \
		curl -L https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip -o external/win/raylib.zip; \
		unzip external/win/raylib.zip -d external/win; \
		mv external/win/raylib-5.5_win64_mingw-w64 external/win/raylib; \
		rm external/win/raylib.zip; \
	fi

build/win/fps-raylib.exe: external/win assets src/yari/*.c src/yari/platform/raylib/*.c
	@mkdir -p build/win
	$(WINCC) $(WIN_CFLAGS) -o build/win/fps-raylib.exe example/fps/main/main.c src/yari/*.c src/yari/platform/raylib/*.c $(WIN_LIBS)

build/win/kart-raylib.exe: external/win assets src/yari/*.c src/yari/platform/raylib/*.c
	@mkdir -p build/win
	$(WINCC) $(WIN_CFLAGS) -o build/win/kart-raylib.exe example/kart/main/main.c src/yari/*.c src/yari/platform/raylib/*.c $(WIN_LIBS)

build/win/map_builder.exe: external/win src/tools/map_builder.c
	@mkdir -p build/win
	$(WINCC) $(WIN_CFLAGS) -I./src/tools -o build/win/map_builder.exe src/tools/map_builder.c $(WIN_LIBS)

.PHONY: build-win build-map-builder-win edit-fps-win edit-fps2-win edit-kart-win run-win run-kart-win
build-win: build/win/fps-raylib.exe
build-kart-win: build/win/kart-raylib.exe
build-map-builder-win: build/win/map_builder.exe

edit-fps-win: build/win/map_builder.exe
	build/win/map_builder.exe example/fps/assets/ example/fps/main/level1.h
edit-fps2-win: build/win/map_builder.exe
	build/win/map_builder.exe example/fps/assets/ example/fps/main/level2.h
edit-kart-win: build/win/map_builder.exe
	build/win/map_builder.exe example/kart/assets/ example/kart/main/level.h

run-win: build/win/fps-raylib.exe
	build/win/fps-raylib.exe
run-kart-win: build/win/map_builder.exe
	build/win/map_builder.exe
