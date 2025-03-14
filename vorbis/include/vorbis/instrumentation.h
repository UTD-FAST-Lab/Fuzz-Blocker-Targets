#include "instrumentation.h"

void foo() {
    __sanitizer_cov_trace_pc_guard(&guards[4]);
    printf("Inside foo\n");  // Line 4
}

int main() {
    initialize_guards();  // Ensure guards are initialized before usage
    __sanitizer_cov_trace_pc_guard(&guards[8]);
    printf("Starting program...\n");
    foo();
    __sanitizer_cov_trace_pc_guard(&guards[9]);
    printf("Ending program...\n");
    return 0;
}
