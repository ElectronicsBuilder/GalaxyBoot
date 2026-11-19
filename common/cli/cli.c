/*
 * cli.c - text command console. See cli.h.
 * No hardware access: output goes through the writer of each port.
 */
#include "cli.h"
#include "cli_parse.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_LINE_MAX   160
#define CLI_MAX_TABLES 12
#define CLI_PROMPT     "> "

typedef struct {
  CliWriter write;
  char      line[CLI_LINE_MAX];
  size_t    line_len;
  char      last_line[CLI_LINE_MAX];  /* Up-arrow recall */
  int       machine;                  /* 1 = no echo / prompt */
  int       esc_state;                /* 0, 1 = ESC seen, 2 = ESC [ seen */
  char      prev_char;
} Port;

static const CliCommand *tables[CLI_MAX_TABLES];
static size_t table_sizes[CLI_MAX_TABLES];
static size_t table_count;

static Port ports[CLI_MAX_PORTS];
static Port *cur = &ports[0];          /* port of the command being run */
static char error_text[96];
static int executing;                  /* a command is running: no prompt redraw */
static const char *banner_text = "";

static void P_WriteTo(Port *p, const char *s)
{
  if (p->write != NULL) p->write(s, strlen(s));
}

static void P_Write(const char *s)
{
  P_WriteTo(cur, s);
}

static void P_PutC(char c)
{
  char s[2] = { c, '\0' };
  P_Write(s);
}

static void P_PromptOf(Port *p)
{
  if (!p->machine && !(executing && p == cur))
  {
    P_WriteTo(p, CLI_PROMPT);
    p->line[p->line_len] = '\0';
    P_WriteTo(p, p->line);
  }
}

int Cli_Register(const CliCommand *table, size_t count)
{
  if (table_count >= CLI_MAX_TABLES) return -1;
  tables[table_count] = table;
  table_sizes[table_count] = count;
  table_count++;
  return 0;
}

void Cli_SetWriter(int port, CliWriter writer)
{
  if (port >= 0 && port < CLI_MAX_PORTS) ports[port].write = writer;
}

int Cli_CurrentPort(void)
{
  return (int)(cur - ports);
}

void Cli_Printf(const char *fmt, ...)
{
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  P_Write(buf);
  P_Write("\r\n");
}

int Cli_Error(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(error_text, sizeof error_text, fmt, ap);
  va_end(ap);
  return -1;
}

int Cli_IsMachine(void)      { return cur->machine; }
void Cli_SetMachine(int on)  { cur->machine = on ? 1 : 0; }

int Cli_ParseInt(const char *s, long min, long max, long *out)
{
  char *end;
  if (s == NULL || *s == '\0') return -1;
  long v = strtol(s, &end, 0);
  if (*end != '\0' || v < min || v > max) return -1;
  *out = v;
  return 0;
}

void Cli_Log(const char *text)
{
  for (int i = 0; i < CLI_MAX_PORTS; i++)
  {
    Port *p = &ports[i];
    if (p->write == NULL || p->machine) continue;     /* machine mode: replies only */
    int typing = !(executing && p == cur);
    if (typing) P_WriteTo(p, "\r\x1b[K");             /* clear the prompt line */
    P_WriteTo(p, text);
    P_WriteTo(p, "\r\n");
    if (typing) P_PromptOf(p);                        /* prompt + partly typed command */
  }
}

static const CliCommand *P_Find(const char *name)
{
  for (size_t t = 0; t < table_count; t++)
  {
    for (size_t i = 0; i < table_sizes[t]; i++)
    {
      if (strcmp(tables[t][i].name, name) == 0) return &tables[t][i];
    }
  }
  return NULL;
}

static void P_Execute(const char *command_line)
{
  static char text[CLI_LINE_MAX];   /* tokenized copy */
  char *argv[CLI_MAX_ARGS];
  strncpy(text, command_line, sizeof text - 1u);
  text[sizeof text - 1u] = '\0';
  int argc = Cli_Tokenize(text, argv, CLI_MAX_ARGS);
  if (argc == 0) return;

  const CliCommand *cmd = P_Find(argv[0]);
  error_text[0] = '\0';
  if (cmd == NULL)
  {
    Cli_Printf("ERR unknown command '%s' (type help)", argv[0]);
    return;
  }
  executing = 1;
  int rc = cmd->run(argc, argv);
  executing = 0;
  if (rc == 0) Cli_Printf("OK");
  else Cli_Printf("ERR %s", error_text[0] ? error_text : "failed");
}

void Cli_ExecuteLine(int port, const char *line)
{
  if (port < 0 || port >= CLI_MAX_PORTS) return;
  cur = &ports[port];
  P_Execute(line);
}

/* print a text with several lines (split at \n) */
static void P_PrintLines(const char *text, const char *indent)
{
  char line[160];
  while (*text)
  {
    size_t n = strcspn(text, "\n");
    if (n >= sizeof line) n = sizeof line - 1u;
    memcpy(line, text, n);
    line[n] = '\0';
    Cli_Printf("%s%s", indent, line);
    text += n;
    if (*text == '\n') text++;
  }
}

/* help [command]: list commands or show one command */
static int P_CmdHelp(int argc, char **argv)
{
  if (argc > 1)
  {
    const CliCommand *cmd = P_Find(argv[1]);
    if (cmd == NULL) return Cli_Error("unknown command '%s' (type help)", argv[1]);
    Cli_Printf("%s - %s", cmd->name, cmd->help);
    if (cmd->usage != NULL) P_PrintLines(cmd->usage, "  ");
    else Cli_Printf("  %s %s", cmd->name, cmd->args);
    return 0;
  }
  Cli_Printf("Commands (help <command> for details and examples):");
  for (size_t t = 0; t < table_count; t++)
  {
    for (size_t i = 0; i < table_sizes[t]; i++)
    {
      char left[40];
      snprintf(left, sizeof left, "%s %s", tables[t][i].name, tables[t][i].args);
      Cli_Printf("  %-26s %s", left, tables[t][i].help);
    }
  }
  Cli_Printf("Typing: Backspace, Up = repeat last command, Ctrl-C = clear line,");
  Cli_Printf("  \"quotes\" for text with spaces. Replies end with OK or ERR <reason>.");
  return 0;
}

/* echo on|off: human / machine mode of this port */
static int P_CmdEcho(int argc, char **argv)
{
  if (argc < 2) return Cli_Error("usage: echo on|off");
  if (strcmp(argv[1], "on") == 0) Cli_SetMachine(0);
  else if (strcmp(argv[1], "off") == 0) Cli_SetMachine(1);
  else return Cli_Error("usage: echo on|off");
  return 0;
}

static const CliCommand cli_builtin[] = {
  { "help", "[command]", "this list; details of a command", P_CmdHelp, NULL },
  { "echo", "on|off", "on = human mode, off = machine mode (no echo, no prompt)", P_CmdEcho, NULL },
};

void Cli_Init(const char *banner)
{
  banner_text = banner ? banner : "";
  Cli_Register(cli_builtin, sizeof cli_builtin / sizeof cli_builtin[0]);
  for (int i = 0; i < CLI_MAX_PORTS; i++)
  {
    Port *p = &ports[i];
    if (p->write == NULL) continue;
    P_WriteTo(p, "\r\n");
    P_WriteTo(p, banner_text);
    P_WriteTo(p, " - type 'help' for commands.\r\n");
    P_PromptOf(p);
  }
}

static void P_Backspace(void)
{
  if (cur->line_len > 0)
  {
    cur->line_len--;
    if (!cur->machine) P_Write("\b \b");
  }
}

void Cli_InputFrom(int port, char c)
{
  if (port < 0 || port >= CLI_MAX_PORTS) return;
  cur = &ports[port];
  Port *p = cur;

  /* ANSI escape sequences (arrow keys) */
  if (p->esc_state == 1) { p->esc_state = (c == '[') ? 2 : 0; return; }
  if (p->esc_state == 2)
  {
    p->esc_state = 0;
    if (c == 'A' && p->last_line[0] != '\0')   /* Up: recall last command */
    {
      while (p->line_len > 0) P_Backspace();
      p->line_len = strlen(p->last_line);
      memcpy(p->line, p->last_line, p->line_len + 1u);
      if (!p->machine) P_Write(p->line);
    }
    return;
  }

  switch (c)
  {
    case 0x1B:                        /* ESC */
      p->esc_state = 1;
      break;

    case '\r':
    case '\n':
      if (c == '\n' && p->prev_char == '\r') break;   /* CR LF counts once */
      if (!p->machine) P_Write("\r\n");
      p->line[p->line_len] = '\0';
      if (p->line_len > 0)
      {
        memcpy(p->last_line, p->line, p->line_len + 1u);
        p->line_len = 0;
        P_Execute(p->last_line);
      }
      P_PromptOf(p);
      break;

    case '\b':
    case 0x7F:                        /* Backspace / Delete */
      P_Backspace();
      break;

    case 0x03:                        /* Ctrl-C */
      p->line_len = 0;
      if (!p->machine) P_Write("^C\r\n");
      P_PromptOf(p);
      break;

    default:
      if (c >= 0x20 && c < 0x7F && p->line_len < CLI_LINE_MAX - 1u)
      {
        p->line[p->line_len++] = c;
        if (!p->machine) P_PutC(c);
      }
      break;
  }
  p->prev_char = c;
}
