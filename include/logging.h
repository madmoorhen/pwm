/* Include guard */
#ifndef LOGGING_H
#define LOGGING_H

/* Config */
#define LOGS 1      /* Enable logging */
#define ANSI_LOGS 1 /* Enable formatted logs with ANSI escape codes */

/* Log levels */
typedef enum {
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_ERROR
} log_level_t;


/* Log to stdout */
extern void log_msg(log_level_t level, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* LOGGING_H */
