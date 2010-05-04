#include <math.h>

#include "demo.h"

#define NOTCH  1/8.0
#define SCALE  1/16.0

float polygon_ship_v[7][2];

void polygon_load_ship(struct polygon *polygon) {
  float (*v)[2] = polygon_ship_v;

  float x = 1/2.0 * -sin(2*M_PI * 1 / 3.0);
  float y = 1/2.0 *  cos(2*M_PI * 1 / 3.0);

  v[0][0] =           SCALE * (-x);
  v[1][0] =           SCALE * ( 0);
  v[2][0] =           SCALE * ( x);

  v[1][1] =           SCALE * ( 1/2.0);
  v[2][1] = v[0][1] = SCALE * ( y);

  v[4][0] = v[3][0] = SCALE * (-NOTCH / 2.0);
  v[6][0] = v[5][0] = SCALE * ( NOTCH / 2.0);

  v[6][1] = v[3][1] = SCALE * ( y);
  v[5][1] = v[4][1] = SCALE * ( y + NOTCH);

  polygon->n = 7;
  polygon->v = v;
}
