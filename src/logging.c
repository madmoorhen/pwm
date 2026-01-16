/* Implements logging.h */
#include <logging.h>

/* Includes */
#include <stdarg.h> /* For va_list */
#include <stdio.h>  /* For fprintf(), vfprintf() */
#include <stdlib.h> /* For abort() */

/* Constants */
const char *LOG_LEVELS[] = {
#if ANSI_LOGS
  [LOG_LEVEL_INFO] = "\x1b[1;4;96mINFO\x1b[0m: ",
  [LOG_LEVEL_WARNING] = "\x1b[1;4;93mWARNING\x1b[0m: ",
  [LOG_LEVEL_ERROR] = "\x1b[1;4;91mERROR\x1b[0m: ",
#else
#define ADD_LEVEL(lvl) [LOG_LEVEL_##lvl] = #lvl ": ",
  ADD_LEVEL(INFO)
  ADD_LEVEL(WARNING)
  ADD_LEVEL(ERROR)
#undef ADD_LEVEL
#endif
};

/* Log to stdout */
void log_msg(log_level_t level, const char *format, ...) {
#if LOGS
  fprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, LOG_LEVELS[level]);
  va_list args;
  va_start(args, format);
  vfprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, format, args);
  va_end(args);
  fprintf(level == LOG_LEVEL_ERROR ? stderr : stdout, "\n");
  if (level == LOG_LEVEL_ERROR) abort();
#endif
}
