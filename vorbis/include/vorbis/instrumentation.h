#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdio.h>
#include <stdint.h>
#include <sanitizer/coverage_interface.h> 

extern uint8_t guards[];  // Declare the guards array

void initialize_guards();  // Function to initialize guards
void __sanitizer_cov_trace_pc_guard(unsigned int *guard);  // Function to track execution

#endif  // INSTRUMENTATION_H
