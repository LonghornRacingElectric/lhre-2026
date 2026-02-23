#include "low_pass.h"

void ema_filter_init(ema_filter_t *filter) {
  filter->value = 0.0f;
  filter->initialized = false;
}

float ema_filter_evaluate(ema_filter_t *filter, float input, float alpha) {
  if (!filter->initialized) {
    filter->value = input;
    filter->initialized = true;
  } else {
    filter->value = (alpha * input) + ((1.0f - alpha) * filter->value);
  }
  return filter->value;
}
