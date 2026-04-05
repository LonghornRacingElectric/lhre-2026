#include "SurfaceParameter.h"
#include <cmath>
#include <cstring>

/**
 * Initialize surface as an 11x11 matrix.
 *
 * @param x0 minimum x, corresponds to first column, z[y][0]
 * @param x1 maximum x, corresponds to last column, z[y][10]
 * @param y0 minimum y, corresponds to first row, z[0][x]
 * @param y1 maximum y, corresponds to last row, z[10][x]
 * @param z 11x11 matrix of f(x, y) where row index corresponds to y and column index to x
 */
SurfaceParameter::SurfaceParameter(float x0, float x1, float y0, float y1, float z[11][11]) {
    this->x0 = x0;
    this->x1 = x1;
    this->y0 = y0;
    this->y1 = y1;
    memcpy(this->z, z, sizeof(float) * 11 * 11);
}

float SurfaceParameter::operator()(float x, float y) {
    float xpct = (x - x0) / (x1 - x0) * 10.0f;
    xpct = fminf(xpct, 10.0f);
    xpct = fmaxf(xpct, 0.0f);

    int xi = (int) floorf(xpct);
    int xj = (int) ceilf(xpct);

    float ypct = (y - y0) / (y1 - y0) * 10.0f;
    ypct = fminf(ypct, 10.0f);
    ypct = fmaxf(ypct, 0.0f);

    int yi = (int) floorf(ypct);
    int yj = (int) ceilf(ypct);

    float a = xpct - (float) xi;
    float z0 = ((1-a) * z[yi][xi]) + (a * z[yi][xj]);
    float z1 = ((1-a) * z[yj][xi]) + (a * z[yj][xj]);

    float b = ypct - (float) yi;
    float zr = ((1-b) * z0) + (b * z1);

    return zr;
}
