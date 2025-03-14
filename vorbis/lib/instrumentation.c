#include "instrumentation.h"

uint8_t guards[9999] = {1};  // Define the guards array

void initialize_guards() {
    for (int i = 0; i < 9999; i++)
        guards[i] = 1;
}

void __sanitizer_cov_trace_pc_guard(unsigned int *guard) {
    if (!*guard) return;  // Skip if uninitialized
    void *PC = __builtin_return_address(0);
    printf("[TRACE] Executed line at address: %p\n", PC);
}
