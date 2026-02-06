CFLAGS = -Wall -Wextra -O2 -DDEBUG $(shell pkg-config --cflags raylib)
LIBS = $(shell pkg-config --libs raylib) -lm

all: ray

build_assets: tools/assets_packer.c
	$(CC) -o build/assets_packer tools/assets_packer.c

assets: build_assets
	build/assets_packer assets main/assets.h

ray: assets main/main.c
	$(CC) $(CFLAGS) -o build/ray main/main.c $(LIBS)

run: ray
	build/ray
