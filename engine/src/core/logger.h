#pragma once

#include "defines.h"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

// Disable logging for release builds
#if P_RELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif // PRELEASE

typedef enum log_level {
  LOG_LEVEL_FATAL = 0,
  LOG_LEVEL_ERROR = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_INFO = 3,
  LOG_LEVEL_DEBUG = 4,
  LOG_LEVEL_TRACE = 5
} log_level;

b8 initialize_logging();
void shutdown_loggin();

P_API void log_output(log_level level, const char* message, ...);

#ifndef P_FATAL
#define P_FATAL(message, ...) log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__);
#endif

#ifndef P_ERROR
// log error-level msg
#define P_ERROR(message, ...) log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);
#endif // P_ERROR

#if LOG_WARN_ENABLED == 1
// log warning-level msg
#define P_WARN(message, ...) log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
#define P_WARN(message ....)
#endif

#if LOG_INFO_ENABLED == 1
#define P_INFO(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
#define P_INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define P_DEBUG(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
#define P_DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define P_TRACE(message, ...) log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
#define P_TRACE(message, ...)
#endif