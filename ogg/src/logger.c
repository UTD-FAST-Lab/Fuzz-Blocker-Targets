#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <ogg/logger.h>

// Global log file and mutex
FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize log file
void log_init(const char *filename) {
    log_file = fopen(filename, "a");
}

// Write a message to the log
void log_message(const char *message) {
    if (!log_file) return;

    pthread_mutex_lock(&log_mutex);

    // Get timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_file, "[%02d:%02d:%02d] %s\n", t->tm_hour, t->tm_min, t->tm_sec, message);

    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}

// Close log file
void log_close() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
