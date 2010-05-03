#include <cairo.h>
#include <math.h>

#include "SDL.h"
#include "demo.h"

#define WIDTH   320
#define HEIGHT  240
#define SCALE   sqrt(HEIGHT * WIDTH)
#define FPS     30

#define SHIP_MAX      4
#define JOYSTICK_MAX  4

#define SHIP_ACCEL   (1/1.0)
#define SHIP_DRAG    (3/4.0)
#define SHIP_TACCEL  (4/1.0)
#define SHIP_TDRAG   (63/64.0)

struct polygon polygon;

struct joystick {
    int is_alive;
    SDL_Joystick *sdl;
    int ship;
  } joysticks[JOYSTICK_MAX];

struct ship {
    int is_alive;
    float x, y, a, xv, yv, av;
    float steer, gas;
  } ships[SHIP_MAX];

float frand() {
  return (float) rand() / RAND_MAX;
}

int main(int argc, char **argv) {
  SDL_Surface *sdl_surface;
  Uint32 next_frame;
  cairo_t *cr;
  cairo_matrix_t cairo_matrix_display;

  int running;

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK);
  SDL_ShowCursor(0);
  SDL_SetVideoMode(WIDTH, HEIGHT, 32, 0);
  sdl_surface = SDL_GetVideoSurface();

  { /* Initialize Canvas */
    cairo_surface_t *cr_surface;
    cr_surface = cairo_image_surface_create_for_data(
      sdl_surface->pixels,
      CAIRO_FORMAT_RGB24,
      sdl_surface->w,
      sdl_surface->h,
      sdl_surface->pitch
      );
    cr = cairo_create(cr_surface);
    cairo_surface_destroy(cr_surface);
  }

  {
    cairo_matrix_t *m = &cairo_matrix_display;

    cairo_matrix_init_identity(m);

    // Cartesian
    cairo_matrix_translate(m, WIDTH/2.0, HEIGHT/2.0);
    cairo_matrix_scale(m,  1, -1);

    // fixed scale
    cairo_matrix_scale(m, SCALE, SCALE);
  }

  /* Initialize Delay */
  next_frame = 1024.0 / FPS;

  { /* Game Logic */
    int i;

    running = 1;

    polygon_load_ship(&polygon);

    for (i = 0; i < JOYSTICK_MAX; i = i + 1) {
      SDL_Joystick *sdl_joystick;

      sdl_joystick = SDL_JoystickOpen(i);
      if (sdl_joystick) {
        joysticks[i].is_alive = 0;
        joysticks[i].sdl = sdl_joystick;
      } else {
        joysticks[i].is_alive = 0;
      }
    }

    for (i = 0; i < SHIP_MAX; i = i + 1) {
      ships[i].is_alive = 0;
    }
  }

  SDL_LockSurface(sdl_surface);
  while (running) {

    { /* Render Frame */
      int i, j;

      // Clear Screen
      cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint(cr);
      cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

      // Draw Ships
      for (j = 0; j < SHIP_MAX; j = j + 1) {
        if (! ships[j].is_alive) { continue; }

        cairo_set_matrix(cr, &cairo_matrix_display);
        cairo_translate(cr, ships[j].x, ships[j].y);
        cairo_rotate(cr, ships[j].a);
        cairo_new_sub_path(cr);
        for (i = 0; i < polygon.n; i = i + 1) {
          cairo_line_to(cr, polygon.v[i][0], polygon.v[i][1]);
        }
        cairo_close_path(cr);
      }

      cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
      cairo_fill(cr);
    }

    /* Update Display */
    SDL_UnlockSurface(sdl_surface);
    SDL_Flip(sdl_surface);
    SDL_LockSurface(sdl_surface);

    { /* Delay */
      Uint32 now;

      now = SDL_GetTicks();
      if (now < next_frame) {
        SDL_Delay(next_frame - now);
      }
      next_frame = next_frame + 1024.0 / FPS;
    }

    { /* Game Logic */
      SDL_Event event;
      int i, j;

      while ( SDL_PollEvent(&event) ) {
        if (event.type == SDL_KEYDOWN) {
          if (event.key.keysym.sym == SDLK_q) {
            running = 0;
          }
        }
      }

      for (j = 0; j < JOYSTICK_MAX; j = j + 1) {
        if (! joysticks[j].sdl) { continue; }

        if (joysticks[j].is_alive) {
          struct ship *ship = &ships[joysticks[j].ship];
          SDL_Joystick *joystick = joysticks[j].sdl;

          float steer = 0;
          float gas   = 0;

          /* Input */

          // Xbox 360 Controller
          steer = steer \
            - SDL_JoystickGetAxis(joystick, 0) / 32768.0;
          gas   = gas   \
            + ( SDL_JoystickGetAxis(joystick, 4) / 32768.0 + 1 ) / 2.0;

          /* Normalization */

          if (steer < -1) { steer = -1; }
          if (steer >  1) { steer =  1; }
          if (gas   >  1) { gas   =  1; }

          ship->steer = steer;
          ship->gas   = gas;
        } else {
          if (! SDL_JoystickGetButton(joysticks[j].sdl, 0) ) { continue; }

printf("foo\n");

          for (i = 0; i < SHIP_MAX; i = i + 1) {
            if (! ships[i].is_alive) { break; }
          }
          if (i == SHIP_MAX) {
            error(0, 0, "Out of ships.");
            continue;
          }
          joysticks[j].is_alive = 1;
          joysticks[j].ship = i;

          ships[i].is_alive = 1;
          ships[i].x = WIDTH/SCALE * ( frand() - 1/2.0 );
          ships[i].y = HEIGHT/SCALE * ( frand() - 1/2.0 );
          ships[i].a = 2*M_PI * frand();
        }
      }

      for (i = 0; i < SHIP_MAX; i = i + 1) {
        if (! ships[i].is_alive) { continue; };

        // acceleration
        ships[i].xv = ships[i].xv \
          + ships[i].gas   * SHIP_ACCEL  * -sin(ships[i].a) / FPS;
        ships[i].yv = ships[i].yv \
          + ships[i].gas   * SHIP_ACCEL  *  cos(ships[i].a) / FPS;
        ships[i].av = ships[i].av \
          + ships[i].steer * SHIP_TACCEL *  2*M_PI / FPS;

        // velocity
        ships[i].x = ships[i].x + ships[i].xv / FPS;
        ships[i].y = ships[i].y + ships[i].yv / FPS;
        ships[i].a = ships[i].a + ships[i].av / FPS;

        // drag
        ships[i].xv = ships[i].xv * pow(1.0 - SHIP_DRAG,  1.0/FPS);
        ships[i].yv = ships[i].yv * pow(1.0 - SHIP_DRAG,  1.0/FPS);
        ships[i].av = ships[i].av * pow(1.0 - SHIP_TDRAG, 1.0/FPS);
      }
    }

  }
  SDL_UnlockSurface(sdl_surface);

  cairo_destroy(cr);
  SDL_Quit();

  return 0;
}
