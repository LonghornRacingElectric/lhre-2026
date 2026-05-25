#ifndef LOOKUP2D_H
#define LOOKUP2D_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOOKUP2D_POINTS_X 11
#define LOOKUP2D_POINTS_Y 11

typedef struct {
  float x0;
  float x1;
  float y0;
  float y1;
  float z[LOOKUP2D_POINTS_Y][LOOKUP2D_POINTS_X];
} Lookup2D;

/**
 * x = APPS travel
 * y = motor RPM
 * z = torque command
 */
void Lookup2D_init(Lookup2D *lookup2d, float x0, float x1, float y0, float y1,
                   const float z_data[LOOKUP2D_POINTS_Y][LOOKUP2D_POINTS_X]);

float Lookup2D_evaluate(const Lookup2D *lookup2d, float x, float y);

#ifdef __cplusplus
}
#endif

#endif // LOOKUP2D_H