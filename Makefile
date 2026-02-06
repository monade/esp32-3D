CFLAGS = -Wall -Wextra -O2 -DDEBUG $(shell pkg-config --cflags raylib)
LIBS = $(shell pkg-config --libs raylib) -lm

all: ray

ray: main/main.c
	$(CC) $(CFLAGS) -o build/ray main/main.c $(LIBS)

run: ray
	build/ray
