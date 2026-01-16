/* Includes */
#include <logging.h>

/* Entry point */
int main(int argc, char *argv[]) {
  log_msg(LOG_LEVEL_INFO, "UwU >_< %d", 42);
  log_msg(LOG_LEVEL_WARNING, "UwU >_< %d", 42);
  log_msg(LOG_LEVEL_ERROR, "UwU >_< %d", 42);
  return 0;
}
