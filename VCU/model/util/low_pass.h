#ifndef LOW_PASS_H
#define LOW_PASS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float value;
  bool initialized;
} ema_filter_t;

void ema_filter_init(ema_filter_t *filter);

/**
 * @brief Evaluates an Exponential Moving Average low pass filter
 *        using y = alpha * input + (1 - alpha) * y_prev
 *        alpha is the smoothing factor in [0.0 - 1.0]
 */
float ema_filter_evaluate(ema_filter_t *filter, float input, float alpha);

#ifdef __cplusplus
}
#endif

#endif // LOW_PASS_H
