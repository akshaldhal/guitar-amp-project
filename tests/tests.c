#include <logger.h>

void test_log_message() {
  char * message = "Test log message";
  log_message(LOG_LEVEL_INFO, "%s", message);
  log_message(LOG_LEVEL_ERROR, "An error occurred: %d", -1);
  log_message(LOG_LEVEL_DEBUG, "Debugging value: %f", 3.14);
  log_message(LOG_LEVEL_WARN, "This is a warning");
  log_message(LOG_LEVEL_TRACE, "Trace message for detailed debugging");
}

int main() {
  test_log_message();
  return 0;
}