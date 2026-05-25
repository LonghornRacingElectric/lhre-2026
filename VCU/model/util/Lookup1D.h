#ifndef LOOKUP1D_H
#define LOOKUP1D_H

#ifdef __cplusplus
extern "C" {
#endif

#define LOOKUP1D_POINTS 11

typedef struct {
  float x0;
  float x1;
  float y[LOOKUP1D_POINTS];
} Lookup1D;

void Lookup1D_init(Lookup1D *lookup1d, float x0, float x1,
                   const float y_data[LOOKUP1D_POINTS]);

float Lookup1D_evaluate(const Lookup1D *lookup1d, float x);

#ifdef __cplusplus
}
#endif

#endif // LOOKUP1D_H
