#include "logger.h"
#include "assert.h"
#include "platform/platform.h"

// TODO: temporary
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

b8
initialize_logging() {
  // TODO: create a log filea
  return TRUE;
}

void
shutdown_loggin() {
  // TODO: cleanup logging/write queued entries
  return;
}

/*
 * Log the output to the console
*/
void
log_output(log_level level, const char* message, ...) {
  const char* level_strings[6] = {
    "[FATAL]",
    "[ERROR]",
    "[WARN]",
    "[INFO]",
    "[DEBUG]",
    "[TRACE]"
  };

  b8 is_error = level < LOG_LEVEL_WARN;

  // Massive buffer to avoid runtime allocations that are slow
  const i32 msg_length = 32000;
  char err_message[msg_length];
  memset(err_message, 0, sizeof(err_message));

  // Format the message
  __builtin_va_list arg_ptr;
  va_start(arg_ptr, message);
  vsnprintf(err_message, msg_length, message, arg_ptr);
  va_end(arg_ptr);

  // Prepend the error level to the message string
  char out_message[msg_length];
  memset(out_message, 0, sizeof(out_message));
  sprintf(out_message, "%s%s\n", level_strings[level], err_message);

  // Print the error message
  if (is_error) {
    platform_console_write_error(out_message, level);
  } else {
    platform_console_write(out_message, level);
  }
}

// ASSERTIONS //
void
report_assertion_failure(
  const char* expression,
  const char* message,
  const char* file,
  i32 line
) {
  log_output(LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file '%s', line %d\n", expression, message, file, line);
}