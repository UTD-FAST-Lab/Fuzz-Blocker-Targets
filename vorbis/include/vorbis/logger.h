#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

// Log Levels
typedef enum { LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_DEBUG } LogLevel;

extern FILE *log_file;
extern pthread_mutex_t log_mutex;

void log_init(const char *filename);
void log_message(LogLevel level, const char *format, ...);
void log_close();

#endif  // LOGGER_H
