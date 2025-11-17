#include <logger.h>

void log_message(LogLevel level, const char *message, ...) {
  va_list args;
  va_start(args, message);

  switch (level) {
    case LOG_LEVEL_DEBUG:
      printf("\033[0;36m[DEBUG] ");
      break;
    case LOG_LEVEL_INFO:
      printf("\033[0;32m[INFO] ");
      break;
    case LOG_LEVEL_WARN:
      printf("\033[0;33m[WARN] ");
      break;
    case LOG_LEVEL_ERROR:
      printf("\033[0;31m[ERROR] ");
      break;
    default:
      printf("[LOG] ");
      break;
  }

  vprintf(message, args);

  printf("\033[0m\n");

  va_end(args);
}
