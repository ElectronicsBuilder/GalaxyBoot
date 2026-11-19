/*
 * cli.h - text command console, shared by BeaconGalaxy, AudioGalaxy and GalaxyBoot.
 *
 * Each port (e.g. the USB serial console) has its own typing state:
 * Human mode (default): characters are echoed, Backspace edits, Ctrl-C clears
 * the line, Up-arrow recalls the last command, a "> " prompt is shown.
 * Machine mode ("echo off"): no echo and no prompt, for scripts and tools.
 * Every command ends with a line "OK" or "ERR <reason>"; data lines come
 * before it, preferably as "key=value". Replies go to the port the command
 * came from; log lines go to every port in human mode.
 *
 * A port can install a key hook (Cli_SetKeyHook): while it returns non-zero
 * for a byte, the byte is consumed (used e.g. for VT100 arrows -> UI buttons).
 */
#ifndef COMMON_CLI_H
#define COMMON_CLI_H

#include <stddef.h>

#define CLI_MAX_ARGS 12
#define CLI_MAX_PORTS 2

typedef int (*CliHandler)(int argc, char **argv);   /* return 0 = OK, else ERR */

typedef struct {
  const char *name;    /* command word, e.g. "play" */
  const char *args;    /* argument summary for help, e.g. "<index|name>" */
  const char *help;    /* one-line description */
  CliHandler  run;
  const char *usage;   /* "help <command>": details and examples, lines split by \n */
} CliCommand;

/* how a port sends text; NULL = output dropped */
typedef void (*CliWriter)(const char *s, size_t len);

void Cli_Init(const char *banner);                 /* banner shown at start, e.g. "AudioGalaxy 1.0" */
void Cli_SetWriter(int port, CliWriter writer);
void Cli_InputFrom(int port, char c);              /* one received byte (task context) */
int  Cli_Register(const CliCommand *table, size_t count);
void Cli_ExecuteLine(int port, const char *line);  /* run a full command line as if typed */

/* For command handlers (act on the port the command came from) */
void Cli_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));  /* one output line */
int  Cli_Error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));   /* set ERR reason, returns -1 */
int  Cli_IsMachine(void);
void Cli_SetMachine(int on);
int  Cli_CurrentPort(void);

/* Used by log.c: one log line to every port in human mode, keeping a
 * half-typed command line intact */
void Cli_Log(const char *line);

/* Parse helpers for handlers: return 0 on success */
int Cli_ParseInt(const char *s, long min, long max, long *out);

#endif
