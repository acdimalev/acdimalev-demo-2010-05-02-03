#include <cairo.h>
#include <math.h>

#include "SDL.h"
#include "demo.h"

#define RESMUL  (10 * 2 * 2 * 2 * 2)

#define WIDTH   (RESMUL * 8)
#define HEIGHT  (RESMUL * 5)
#define SCALE   sqrt(HEIGHT * WIDTH)
#define FPS     30

#define SHIP_MAX      4
#define JOYSTICK_MAX  4

#define SHIP_ACCEL   (1/8.0)
#define SHIP_DRAG    (1/4.0)
#define SHIP_TACCEL  (4/1.0)
#define SHIP_TDRAG   (63/64.0)
#define SHIP_RADIUS  (1/16.0 / 2.0)

#define BULLET_VELOCITY (1/32.0)
#define BULLET_TIME (1024/4.0)
#define BULLET_RADIUS  (1/64.0 / 2.0)

struct polygon polygons[2];

enum polygon_enum { POLYGON_SHIP, POLYGON_BULLET };

struct joystick {
    int is_alive;
    SDL_Joystick *sdl;
    int ship;
  } joysticks[JOYSTICK_MAX];

struct ship {
    int is_alive;
    float x, y, a, xv, yv, av;
    float steer, gas;
    int shoot;
  } ships[SHIP_MAX];

struct bullet {
    int is_alive;
    Uint32 when;
    float x, y, a;
  } bullets[SHIP_MAX];

float frand() {
  return (float) rand() / RAND_MAX;
}

void polygon_render(
  struct polygon *polygon,
  cairo_t *cr,
  cairo_matrix_t *cm_display,
  cairo_matrix_t *cm_object) {

  int i;

  cairo_set_matrix(cr, cm_display);
  cairo_transform(cr, cm_object);
  cairo_new_sub_path(cr);
  for (i = 0; i < polygon->n; i = i + 1) {
    cairo_line_to(cr, polygon->v[i][0], polygon->v[i][1]);
  }
  cairo_close_path(cr);
}

void polygon_render_wrap(
  struct polygon *polygon,
  cairo_t *cr,
  cairo_matrix_t *cm_display,
  cairo_matrix_t *cm_object) {

  cairo_matrix_t cm_wrap_object;

  cairo_matrix_init_translate(&cm_wrap_object, 0, 0);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
  cairo_matrix_init_translate(&cm_wrap_object, -WIDTH/SCALE, 0);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
  cairo_matrix_init_translate(&cm_wrap_object, 0, -HEIGHT/SCALE);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
  cairo_matrix_init_translate(&cm_wrap_object, -WIDTH/SCALE, -HEIGHT/SCALE);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
}

int main(int argc, char **argv) {
  SDL_Surface *sdl_surface;
  Uint32 next_frame;
  cairo_t *cr;
  cairo_matrix_t cm_display;

  int running;

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK);
  SDL_ShowCursor(0);
  SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_FULLSCREEN);
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
    cairo_matrix_t *m = &cm_display;

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

    polygon_load_ship(&polygons[POLYGON_SHIP]);
    polygon_load_bullet(&polygons[POLYGON_BULLET]);

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
      bullets[i].is_alive = 0;
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
        cairo_matrix_t cm_object;

        if (! ships[j].is_alive) { continue; }

        cairo_matrix_init_identity(&cm_object);
        cairo_matrix_translate(&cm_object, ships[j].x, ships[j].y);
        cairo_matrix_rotate(&cm_object, ships[j].a);

        polygon_render_wrap(&polygons[POLYGON_SHIP],
          cr, &cm_display, &cm_object);
      }

      for (j = 0; j < SHIP_MAX; j = j + 1) {
        cairo_matrix_t cm_object;

        if (! bullets[j].is_alive) { continue; }

        cairo_matrix_init_identity(&cm_object);
        cairo_matrix_translate(&cm_object, bullets[j].x, bullets[j].y);
        cairo_matrix_rotate(&cm_object, bullets[j].a);

        polygon_render_wrap(&polygons[POLYGON_BULLET],
          cr, &cm_display, &cm_object);
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
      Uint32 now;
      int i, j;

      now = SDL_GetTicks();

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

          if (! ship->is_alive) {
            joysticks[j].is_alive = 0;
            continue;
          }

          float steer = 0;
          float gas   = 0;
          int   shoot = 0;

          /* Input */

          // Xbox 360 Controller
          steer = steer \
            - SDL_JoystickGetAxis(joystick, 0) / 32768.0;
          gas   = gas   \
            + ( SDL_JoystickGetAxis(joystick, 4) / 32768.0 + 1 ) / 2.0;
          shoot = shoot + SDL_JoystickGetButton(joystick, 0);

          /* Normalization */

          if (steer < -1) { steer = -1; }
          if (steer >  1) { steer =  1; }
          if (gas   >  1) { gas   =  1; }
          if (shoot >  1) { shoot =  1; }

          ship->steer = steer;
          ship->gas   = gas;
          ship->shoot = shoot;
        } else {
          if (! SDL_JoystickGetButton(joysticks[j].sdl, 0) ) { continue; }

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
          ships[i].x = WIDTH/SCALE * frand();
          ships[i].y = HEIGHT/SCALE * frand();
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

        // wrap
        if (ships[i].x < 0) {
          ships[i].x = ships[i].x + WIDTH/SCALE;
        }
        if (ships[i].x > WIDTH/SCALE) {
          ships[i].x = ships[i].x - WIDTH/SCALE;
        }
        if (ships[i].y < 0) {
          ships[i].y = ships[i].y + HEIGHT/SCALE;
        }
        if (ships[i].y > HEIGHT/SCALE) {
          ships[i].y = ships[i].y - HEIGHT/SCALE;
        }

        // shoot
        if (ships[i].shoot && ! bullets[i].is_alive) {
          bullets[i].is_alive = 1;
          bullets[i].when = now;
          bullets[i].x = ships[i].x;
          bullets[i].y = ships[i].y;
          bullets[i].a = ships[i].a;
        }
      }
      for (i = 0; i < SHIP_MAX; i = i + 1) {
        if (! bullets[i].is_alive) { continue; };

        bullets[i].x = bullets[i].x + BULLET_VELOCITY * -sin(bullets[i].a);
        bullets[i].y = bullets[i].y + BULLET_VELOCITY *  cos(bullets[i].a);

        // wrap
        if (bullets[i].x < 0) {
          bullets[i].x = bullets[i].x + WIDTH/SCALE;
        }
        if (bullets[i].x > WIDTH/SCALE) {
          bullets[i].x = bullets[i].x - WIDTH/SCALE;
        }
        if (bullets[i].y < 0) {
          bullets[i].y = bullets[i].y + HEIGHT/SCALE;
        }
        if (bullets[i].y > HEIGHT/SCALE) {
          bullets[i].y = bullets[i].y - HEIGHT/SCALE;
        }

        // expire
        if (now - bullets[i].when > BULLET_TIME) {
          bullets[i].is_alive = 0;
        }
      }

      for (j = 0; j < SHIP_MAX; j = j + 1) {
        if (! ships[j].is_alive) { continue; };

        for (i = 0; i < SHIP_MAX; i = i + 1) {
          float x = bullets[i].x - ships[j].x;
          float y = bullets[i].y - ships[j].y;

          if (! bullets[i].is_alive) { continue; }

          if (i == j) { continue; }

          if ( sqrt(x*x + y*y) < BULLET_RADIUS + SHIP_RADIUS ) {
            bullets[i].is_alive = 0;
            ships[j].is_alive = 0;
          }
        }
      }
    }

  }
  SDL_UnlockSurface(sdl_surface);

  cairo_destroy(cr);
  SDL_Quit();

  return 0;
}
