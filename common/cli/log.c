/*
 * log.c - leveled log output on the consoles .
 * Lines are "[L] text\r\n" with L = E/W/I/D. Called from the App task.
 */
#include "log.h"
#include "cli.h"
#include <stdarg.h>
#include <stdio.h>

LogLevel log_level = LOG_LEVEL_INFO;

void Log_Printf(LogLevel level, const char *fmt, ...)
{
  static const char tag[] = { 'E', 'W', 'I', 'D' };
  char line[128];

  if (level > log_level)
  {
    return;
  }
  int n = snprintf(line, sizeof line, "[%c] ", tag[level]);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line + n, sizeof line - (size_t)n, fmt, ap);
  va_end(ap);
  Cli_Log(line);              /* every console port, prompt kept intact */
}
