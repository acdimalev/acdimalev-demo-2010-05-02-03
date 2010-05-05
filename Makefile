demo: demo.c demo.h config.h gfx/ship.o gfx/bullet.o
	gcc -o demo demo.c gfx/ship.o gfx/bullet.o `pkg-config --cflags --libs sdl cairo`
