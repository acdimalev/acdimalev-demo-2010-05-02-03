#include <math.h>

#include "demo.h"

#define RATIO  4
#define SCALE  1/64.0

float polygon_bullet_v[4][2];

void polygon_load_bullet(struct polygon *polygon) {
  float (*v)[2] = polygon_bullet_v;

  v[3][0] = v[0][0] = SCALE * (-1/2.0 / RATIO);
  v[2][0] = v[1][0] = SCALE * ( 1/2.0 / RATIO);

  v[1][1] = v[0][1] = SCALE * (-1/2.0);
  v[3][1] = v[2][1] = SCALE * ( 1/2.0);

  polygon->n = 4;
  polygon->v = v;
}
