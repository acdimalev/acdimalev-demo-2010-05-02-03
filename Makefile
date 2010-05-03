demo: demo.c demo.h gfx/ship.o
	gcc -o demo demo.c gfx/ship.o `pkg-config --cflags --libs sdl cairo`
