#include <cairo.h>
#include <math.h>

#include "SDL.h"
#include "demo.h"

#include "config.h"

#define SHIP_MAX      4
#define JOYSTICK_MAX  4

#define SHIP_ACCEL   (1/8.0)
#define SHIP_DRAG    (1/4.0)
#define SHIP_TACCEL  (4/1.0)
#define SHIP_TDRAG   (63/64.0)
#define SHIP_RADIUS  (1/16.0 / 2.0)

#define BULLET_VELOCITY  (1/1.0)
#define BULLET_TIME      (1024/4.0)
#define BULLET_RADIUS    (1/64.0 / 2.0)

#define METEOR_MAX       32
#define METEOR_TYPE_MAX   3

int hres, vres, width, height, pixels;
double aspect, scale;

struct polygon polygons[2];
struct polygon polygons_meteor[METEOR_TYPE_MAX];

enum polygon_enum { POLYGON_SHIP, POLYGON_BULLET };

enum controller_type {
  KEYBOARD,
  JOYSTICK_XBOX360,
  JOYSTICK_PS3
};

/* one keyboard, four gamepads */
#define CONTROLLER_MAX 5

int n_controllers = 0;
struct controller {
  enum controller_type type;
  SDL_Joystick *joystick;
  int is_alive;
  int ship;
} controllers[CONTROLLER_MAX];

int n_joystick_ids = 2;
struct joystick_id {
  char *name;
  enum controller_type type;
} joystick_ids[] = {
  "Xbox Gamepad (userspace driver)", JOYSTICK_XBOX360,
  "Sony PLAYSTATION(R)3 Controller", JOYSTICK_PS3
};

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

struct meteor {
    int is_alive, type;
    float x, y, a;
    float xv, yv, av;
  } meteors[METEOR_MAX];

struct meteor_type {
    int polygon;
    int sides;
    float scale;
    int density;
    float velocity, angular_velocity;
  } meteor_types[METEOR_TYPE_MAX] = {
    0, 3, 1/32.0, 12, 1/4.0,  4/1.0,
    0, 4, 3/32.0,  3, 3/16.0, 1/2.0,
    0, 8, 3/16.0,  1, 1/16.0, 1/8.0
  };

float frand() {
  return (float) rand() / RAND_MAX;
}

int collides(float x1, float y1, float r1, float x2, float y2, float r2) {
  float x = x2 - x1;
  float y = y2 - y1;

  if ( sqrt(x*x + y*y) < r1 + r2 ) {
    return 1;
  }

  return 0;
}

int collides_wrap(float x1, float y1, float r1, float x2, float y2, float r2) {
  int a = 0;

  a = a || collides(x1, y1, r1, x2, y2, r2);
  a = a || collides(x1 + width/scale, y1, r1, x2, y2, r2);
  a = a || collides(x1, y1 + height/scale, r1, x2, y2, r2);
  a = a || collides(x1 + width/scale, y1 + height/scale, r1, x2, y2, r2);

  return a;
}

int ngon(struct polygon *polygon, int n, float scale) {
  float a, aoff;
  float vscale = ( scale + scale / cos(M_PI / n) )/2.0;
  int i;

  polygon->n = n;
  polygon->v = (float (*)[2]) malloc( n * 2 * sizeof(float) );

  for (i = 0; i < n; i = i + 1) {
    a = 2*M_PI * (float) i / n;
    polygon->v[i][0] = vscale * 1/2.0 * -sin(a);
    polygon->v[i][1] = vscale * 1/2.0 *  cos(a);
  }

  return 0;
}

int generate_meteor(int type) {
  float v = meteor_types[type].velocity;
  float av = meteor_types[type].angular_velocity;
  float a = 2*M_PI * frand();
  int i;

  for (i = 0; i < METEOR_MAX; i = i + 1) {
    if (! meteors[i].is_alive) { break; }
  }
  if (i == METEOR_MAX) { return -1; }

  meteors[i].is_alive = 1;
  meteors[i].type = type;

  meteors[i].x = width/scale * frand();
  meteors[i].y = height/scale * frand();
  meteors[i].a = 2*M_PI * frand();

  meteors[i].xv = v * -sin(a);
  meteors[i].yv = v *  cos(a);
  meteors[i].av = 2*M_PI * av * (frand() - 1/2.0);

  return i;
}

int meteor_explode(struct meteor *meteor) {
  int type = meteor->type;
  int density;
  int i, j;

  if (type != 0) {
    density = meteor_types[type-1].density / meteor_types[type].density;
    for (j = 0; j < density; j = j + 1) {
      i = generate_meteor(type - 1);
      if (i == -1) {
        return -1;
      }

      meteors[i].x = meteor->x;
      meteors[i].y = meteor->y;
    }
  }

  meteor->is_alive = 0;

  return 0;
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
  cairo_matrix_init_translate(&cm_wrap_object, -width/scale, 0);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
  cairo_matrix_init_translate(&cm_wrap_object, 0, -height/scale);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
  cairo_matrix_init_translate(&cm_wrap_object, -width/scale, -height/scale);
  cairo_matrix_multiply(&cm_wrap_object, cm_object, &cm_wrap_object);
  polygon_render(polygon, cr, cm_display, &cm_wrap_object);
}

int main(int argc, char **argv) {
  SDL_Surface *sdl_surface;
  SDL_Rect **modes;
  Uint32 next_frame;
  cairo_t *cr;
  cairo_matrix_t cm_display;

  int running;

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK);
  modes = SDL_ListModes(NULL, SDL_FULLSCREEN);
  if (NULL == modes) {
    fprintf(stderr, "no video modes available\n");
    exit(0);
  }
  hres = width  = modes[0]->w;
  vres = height = modes[0]->h;
  aspect = (double) hres / vres;
  pixels = hres * vres;
  scale = sqrt(pixels);
#if FULLSCREEN
  SDL_SetVideoMode(hres, vres, 32, SDL_FULLSCREEN);
  SDL_ShowCursor(0);
#else
  SDL_SetVideoMode(hres, vres, 32, 0);
#endif

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
    cairo_matrix_translate(m, hres/2.0, vres/2.0);
    cairo_matrix_scale(m,  1, -1);

    // fixed scale
    cairo_matrix_scale(m,
      scale * 1.0 * hres / width,
      scale * 1.0 * vres / height);
  }

  { /* Controllers */
    int i, j;

    int n = 0;
    int n_joysticks = SDL_NumJoysticks();

    controllers[n].type = KEYBOARD;
    n = n + 1;

    for (i = 0; i < n_joysticks; i = i + 1) {
      const char *name = SDL_JoystickName(i);

      if (CONTROLLER_MAX == n) {
        fprintf(stderr, "out of controller slots\n");
        break;
      }

      for (j = 0; j < n_joystick_ids; j = j + 1) {
        if (! strcmp(joystick_ids[j].name, name) ) {
          controllers[n].type = joystick_ids[j].type;
          controllers[n].joystick = SDL_JoystickOpen(i);
          controllers[n].is_alive = 0;
          n = n + 1;
          break;
        }
      }

      if (j == n_joystick_ids) {
        fprintf(stderr, "unrecognized joystick: %s\n", name);
      }
    }

    n_controllers = n;
  }

  { /* Game Logic */
    int i;

    running = 1;

    polygon_load_ship(&polygons[POLYGON_SHIP]);
    polygon_load_bullet(&polygons[POLYGON_BULLET]);

    for (i = 0; i < SHIP_MAX; i = i + 1) {
      ships[i].is_alive = 0;
      bullets[i].is_alive = 0;
    }

    for (i = 0; i < METEOR_MAX; i = i + 1) {
      meteors[i].is_alive = 0;
    }

    for (i = 0; i < METEOR_TYPE_MAX; i = i + 1) {
      struct polygon *polygon = &polygons_meteor[i];
      int   sides = meteor_types[i].sides;
      float scale = meteor_types[i].scale;

      if ( ngon(polygon, sides, scale) ) { return -1; }
      meteor_types[i].polygon = i;
    }

    generate_meteor(METEOR_TYPE_MAX - 1);
    generate_meteor(METEOR_TYPE_MAX - 1);
  }

  { /* Initialize Delay */
    Uint32 now;

    now = SDL_GetTicks();
    next_frame = now + 1024.0 / FPS;
  }

  SDL_LockSurface(sdl_surface);
  while (running) {

    { /* Render Frame */
      cairo_matrix_t cm_object;
      int i;

      // Clear Screen
      cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
      cairo_paint(cr);
      cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

      // Draw Ships
      for (i = 0; i < SHIP_MAX; i = i + 1) {
        if (! ships[i].is_alive) { continue; }

        cairo_matrix_init_identity(&cm_object);
        cairo_matrix_translate(&cm_object, ships[i].x, ships[i].y);
        cairo_matrix_rotate(&cm_object, ships[i].a);

        polygon_render_wrap(&polygons[POLYGON_SHIP],
          cr, &cm_display, &cm_object);
      }

      // Draw Bullets
      for (i = 0; i < SHIP_MAX; i = i + 1) {
        if (! bullets[i].is_alive) { continue; }

        cairo_matrix_init_identity(&cm_object);
        cairo_matrix_translate(&cm_object, bullets[i].x, bullets[i].y);
        cairo_matrix_rotate(&cm_object, bullets[i].a);

        polygon_render_wrap(&polygons[POLYGON_BULLET],
          cr, &cm_display, &cm_object);
      }

      // Draw Meteors
      for (i = 0; i < METEOR_MAX; i = i + 1) {
        if (! meteors[i].is_alive) { continue; }

        cairo_matrix_init_identity(&cm_object);
        cairo_matrix_translate(&cm_object, meteors[i].x, meteors[i].y);
        cairo_matrix_rotate(&cm_object, meteors[i].a);

        polygon_render_wrap(
          &polygons_meteor[meteor_types[meteors[i].type].polygon],
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
      Uint32 now = SDL_GetTicks();

      if (now < next_frame) {
        SDL_Delay(next_frame - now);
        next_frame = next_frame + 1024.0 / FPS;
      } else {
        fprintf(stderr, "delay counter reset\n");
        next_frame = now + 1024.0 / FPS;
      }
    }

    { /* Game Logic */
      SDL_Event event;
      Uint32 now;
      int i, j;

      now = SDL_GetTicks();

      while ( SDL_PollEvent(&event) ) {
        if (event.type == SDL_QUIT) {
          running = 0;
        }
        if (event.type == SDL_KEYDOWN) {
          if (event.key.keysym.sym == SDLK_q) {
            running = 0;
          }
        }
      }

      Uint8 *keystate = SDL_GetKeyState(NULL);

      for (j = 0; j < n_controllers; j = j + 1) {
        struct controller *controller = &controllers[j];
        float steer = 0;
        float gas   = 0;
        int   shoot = 0;

        SDL_Joystick *joystick = controller->joystick;

        /* Input */

        switch (controller->type) {
          case KEYBOARD:
            steer = steer + keystate[SDLK_LEFT];
            steer = steer - keystate[SDLK_RIGHT];
            gas   = gas   + keystate[SDLK_UP];
            gas   = gas   + keystate[SDLK_LSHIFT];
            shoot = shoot + keystate[SDLK_LCTRL];
            break;
          case JOYSTICK_XBOX360:
            /* Left Analog */
            steer = steer \
              - SDL_JoystickGetAxis(joystick, 0) / 32767.0;
            /* Right Trigger */
            gas   = gas   \
              + ( SDL_JoystickGetAxis(joystick, 4) / 32767.0 + 1 ) / 2.0;
            /* A Button */
            shoot = shoot + SDL_JoystickGetButton(joystick, 0);
            break;
          case JOYSTICK_PS3:
            /* Left Analog */
            steer = steer \
              - SDL_JoystickGetAxis(joystick, 0) / 32767.0;
            /* D-Pad Left */
            steer = steer \
              + ( SDL_JoystickGetAxis(joystick, 11) / 32767.0 + 1 ) / 2.0;
            /* D-Pad Right */
            steer = steer \
              - ( SDL_JoystickGetAxis(joystick, 9) / 32767.0 + 1 ) / 2.0;
            /* Right Trigger */
            gas   = gas \
              + ( SDL_JoystickGetAxis(joystick, 13) / 32767.0 + 1 ) / 2.0;
            /* X Button */
            shoot = shoot + SDL_JoystickGetButton(joystick, 14);
            break;
        }

        /* Normalization */

        if (steer < -1) { steer = -1; }
        if (steer >  1) { steer =  1; }
        if (gas   >  1) { gas   =  1; }
        if (shoot >  1) { shoot =  1; }

        /* Demo Logic */

        if (controller->is_alive) {
          struct ship *ship = &ships[controller->ship];

          if (! ship->is_alive) {
            controller->is_alive = 0;
            continue;
          }

          ship->steer = steer;
          ship->gas   = gas;
          ship->shoot = shoot;
        } else {
          if (!shoot) { continue; }

          for (i = 0; i < SHIP_MAX; i = i + 1) {
            if (! ships[i].is_alive) { break; }
          }
          if (i == SHIP_MAX) {
            fprintf(stderr, "out of ships\n");
            continue;
          }
          controller->is_alive = 1;
          controller->ship = i;

          ships[i].is_alive = 1;
          ships[i].x = width/scale * frand();
          ships[i].y = height/scale * frand();
          ships[i].a = 2*M_PI * frand();
        }
      }

      /* Physics */

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
          ships[i].x = ships[i].x + width/scale;
        }
        if (ships[i].x > width/scale) {
          ships[i].x = ships[i].x - width/scale;
        }
        if (ships[i].y < 0) {
          ships[i].y = ships[i].y + height/scale;
        }
        if (ships[i].y > height/scale) {
          ships[i].y = ships[i].y - height/scale;
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

        bullets[i].x = bullets[i].x \
          + BULLET_VELOCITY * -sin(bullets[i].a) / FPS;
        bullets[i].y = bullets[i].y \
          + BULLET_VELOCITY *  cos(bullets[i].a) / FPS;

        // wrap
        if (bullets[i].x < 0) {
          bullets[i].x = bullets[i].x + width/scale;
        }
        if (bullets[i].x > width/scale) {
          bullets[i].x = bullets[i].x - width/scale;
        }
        if (bullets[i].y < 0) {
          bullets[i].y = bullets[i].y + height/scale;
        }
        if (bullets[i].y > height/scale) {
          bullets[i].y = bullets[i].y - height/scale;
        }

        // expire
        if (now - bullets[i].when > BULLET_TIME) {
          bullets[i].is_alive = 0;
        }
      }

      for (i = 0; i < METEOR_MAX; i = i + 1) {
        if (!meteors[i].is_alive) { continue; }

        meteors[i].x = meteors[i].x + meteors[i].xv / FPS;
        meteors[i].y = meteors[i].y + meteors[i].yv / FPS;
        meteors[i].a = meteors[i].a + meteors[i].av / FPS;

        if (meteors[i].x < 0) {
          meteors[i].x = meteors[i].x + width/scale;
        }
        if (meteors[i].x > width/scale) {
          meteors[i].x = meteors[i].x - width/scale;
        }
        if (meteors[i].y < 0) {
          meteors[i].y = meteors[i].y + height/scale;
        }
        if (meteors[i].y > height/scale) {
          meteors[i].y = meteors[i].y - height/scale;
        }
      }

      /* Collisions */

      for (j = 0; j < SHIP_MAX; j = j + 1) {
        if (! bullets[j].is_alive) { continue; }

        for (i = 0; i < SHIP_MAX; i = i + 1) {
          if (i == j) { continue; }
          if (! ships[i].is_alive) { continue; };

          if ( collides_wrap(
            bullets[j].x, bullets[j].y, BULLET_RADIUS,
            ships[i].x, ships[i].y, SHIP_RADIUS
            ) ) {

            bullets[j].is_alive = 0;
            ships[i].is_alive = 0;
          }
        }

        for (i = 0; i < METEOR_MAX; i = i + 1) {
          if (! meteors[i].is_alive) { continue; };

          if ( collides_wrap(
            bullets[j].x, bullets[j].y, BULLET_RADIUS,
            meteors[i].x, meteors[i].y,
            meteor_types[meteors[i].type].scale / 2.0
            ) ) {

            bullets[j].is_alive = 0;
            meteor_explode(&meteors[i]);
          }
        }
      }

      for (j = 0; j < METEOR_MAX; j = j + 1) {
        if (! meteors[j].is_alive) { continue; };

        for (i = 0; i < SHIP_MAX; i = i + 1) {
          if (! ships[i].is_alive) { continue; };

          if ( collides_wrap(
            ships[i].x, ships[i].y, SHIP_RADIUS,
            meteors[j].x, meteors[j].y,
            meteor_types[meteors[j].type].scale / 2.0
            ) ) {

            ships[i].is_alive = 0;
            meteor_explode(&meteors[j]);
          }
        }
      }

      for (i = 0; i < METEOR_MAX; i = i + 1) {
        if (meteors[i].is_alive) { break; }
      }
      if (i == METEOR_MAX) {
        generate_meteor(METEOR_TYPE_MAX - 1);
        generate_meteor(METEOR_TYPE_MAX - 1);
      }

    }

  }
  SDL_UnlockSurface(sdl_surface);

  {
    int i;

    for (i = 0; i < METEOR_TYPE_MAX; i = i + 1) {
      free(polygons_meteor[i].v);
    }
  }

  cairo_destroy(cr);
  SDL_Quit();

  return 0;
}
