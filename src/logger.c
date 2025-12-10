#include <logger.h>

#define DEBUG_MODE 1

void log_message(LogLevel level, const char *message, ...) {
  va_list args;
  va_start(args, message);
  FILE *stream = stdout;

  switch (level) {
    case LOG_LEVEL_DEBUG:
      if (DEBUG_MODE){
        fprintf(stream, "\033[0;36m[DEBUG] ");
      } else {
        va_end(args);
        return;
      }
      break;
    case LOG_LEVEL_INFO:
      fprintf(stream, "\033[0;32m[INFO] ");
      break;
    case LOG_LEVEL_WARN:
      fprintf(stream, "\033[0;33m[WARN] ");
      break;
    case LOG_LEVEL_ERROR:
      stream = stderr;
      fprintf(stream, "\033[0;31m[ERROR] ");
      break;
    case LOG_LEVEL_TRACE:
      fprintf(stream, "\033[0;35m[TRACE] ");
      break;
    default:
      fprintf(stream, "[LOG] ");
      break;
  }

  vfprintf(stream, message, args);

  fprintf(stream, "\033[0m\n");

  va_end(args);
}
