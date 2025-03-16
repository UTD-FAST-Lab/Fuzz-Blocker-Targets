#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Initialize log file
void log_init(const char *filename);

// Write a message to the log
void log_message(const char *message);

// Close log file
void log_close();

#endif  // LOGGER_H
