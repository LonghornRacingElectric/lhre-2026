#ifndef VCU_CORE_BSEPROCESSOR_H
#define VCU_CORE_BSEPROCESSOR_H

#include <stdint.h>

#include "VcuParameters.h"
#include "util/filters/Timer.h"
#include "util/filters/LowPassFilter.h"

#define BSE_OK 0x00
#define BSE_1_OUT_OF_BOUNDS 0x01
#define BSE_2_OUT_OF_BOUNDS 0x02
#define BSE_BOTH_OUT_OF_BOUNDS 0x04

#define BSE_SHUTDOWN_MASK (BSE_BOTH_OUT_OF_BOUNDS)

typedef struct BseProcessorInput {
  float bse1; // bse1 voltage (V)
  float bse2; // bse2 voltage (V)
} BseProcessorInput;

typedef struct BseProcessorOutput {
  float bse1; // brake pressure (psi)
  float bse2; // brake pressure (psi)
  float bse; // brake pressure (psi)
  uint32_t fault;
  bool ok;
} BseProcessorOutput;

class BseProcessor {
public:
  void reset();

  void setParameters(VcuParameters *params);

  void evaluate(VcuParameters *params, BseProcessorInput *input, BseProcessorOutput *output, float deltaTime);

private:
  Timer clock = Timer(0);
  LowPassFilter bse1Filter = LowPassFilter(0);
  LowPassFilter bse2Filter = LowPassFilter(0);
};


#endif //VCU_CORE_BSEPROCESSOR_H
