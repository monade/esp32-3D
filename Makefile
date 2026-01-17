CFLAGS = -Wall -Wextra -I/opt/homebrew/Cellar/raylib/5.5/include -O2 -DDEBUG
CLIBS = -L/opt/homebrew/Cellar/raylib/5.5/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

all: cray

config:
	brew install raylib

cray: main/main.c
	$(CC) $(CFLAGS) -o build/ray main/main.c $(CLIBS)

run: cray
	build/ray
