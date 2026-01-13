CFLAGS = -Wall -Wextra -I/opt/homebrew/Cellar/raylib/5.5/include -O2
CLIBS = -L/opt/homebrew/Cellar/raylib/5.5/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

all: cray

config:
	brew install raylib
	mkdir -p build

cray: main/main.c
	$(CC) $(CFLAGS) -o build/ray main/main.c $(CLIBS)

ray: cray
	build/ray
