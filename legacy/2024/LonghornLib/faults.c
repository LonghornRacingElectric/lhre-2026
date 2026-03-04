#include "faults.h"

uint32_t faultVector = 0;

void fault_set(uint32_t* fault_vector, uint32_t fault) {
  *fault_vector |= fault;
}

void clear_fault(uint32_t* fault_vector, uint32_t fault) {
  *fault_vector &= ~fault;
}

void clear_all_faults(uint32_t* fault_vector) {
  *fault_vector = 0;
}

bool check_fault(const uint32_t* fault_vector, uint32_t fault) {
  return (*fault_vector & fault) != 0;
}
