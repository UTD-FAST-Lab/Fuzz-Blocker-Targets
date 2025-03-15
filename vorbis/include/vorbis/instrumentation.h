#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Global array for tracking execution
extern unsigned int guards[9999];

// Function to initialize guards
void initialize_guards();

// Function to track execution
void __sanitizer_cov_trace_pc_guard(unsigned int *guard);

#ifdef __cplusplus
}
#endif

#endif  // INSTRUMENTATION_H
