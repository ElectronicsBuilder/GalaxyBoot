/*
 * log.h - leveled log output on the console ports.
 *
 *   LOG_E("EEPROM not responding");          -> "[E] EEPROM not responding"
 *   LOG_I("Game started: %s vs %s", a, b);    -> "[I] Game started: ..."
 *
 * The runtime level (log_level) hides less important lines; LOG_D is compiled
 * out unless LOG_ENABLE_DEBUG is 1.
 */
#ifndef COMMON_LOG_H
#define COMMON_LOG_H

#include <stdint.h>

typedef enum {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_WARN  = 1,
  LOG_LEVEL_INFO  = 2,
  LOG_LEVEL_DEBUG = 3
} LogLevel;

#ifndef LOG_ENABLE_DEBUG
#define LOG_ENABLE_DEBUG 0
#endif

extern LogLevel log_level;   /* default LOG_LEVEL_INFO */

void Log_Printf(LogLevel level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define LOG_E(...) Log_Printf(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_W(...) Log_Printf(LOG_LEVEL_WARN,  __VA_ARGS__)
#define LOG_I(...) Log_Printf(LOG_LEVEL_INFO,  __VA_ARGS__)
#if LOG_ENABLE_DEBUG
#define LOG_D(...) Log_Printf(LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define LOG_D(...) ((void)0)
#endif

#endif
