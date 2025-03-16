#include "logger.h"
#include <stdarg.h>

FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char *filename) {
    if (!log_file) {
        log_file = fopen(filename, "a"); // Append mode
        if (!log_file) {
            fprintf(stderr, "Error: Cannot open log file: %s\n", filename);
            exit(1);
        }
    }
}

void log_message(LogLevel level, const char *format, ...) {
    if (!log_file) return;

    pthread_mutex_lock(&log_mutex);

    // Get current timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_file, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);

    // Log level prefix
    switch (level) {
        case LOG_INFO:    fprintf(log_file, "[INFO] "); break;
        case LOG_WARNING: fprintf(log_file, "[WARNING] "); break;
        case LOG_ERROR:   fprintf(log_file, "[ERROR] "); break;
        case LOG_DEBUG:   fprintf(log_file, "[DEBUG] "); break;
    }

    // Print message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);

    pthread_mutex_unlock(&log_mutex);
}

void log_close() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
