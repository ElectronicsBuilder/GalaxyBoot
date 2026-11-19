/*
 * cli_parse.c - command line tokenizer (no hardware access; tested on the PC).
 */
#include "cli_parse.h"

/* Split a line into words; "double quoted" words may contain spaces, \" and \\. */
int Cli_Tokenize(char *s, char **argv, int max_args)
{
  int argc = 0;
  while (*s && argc < max_args)
  {
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '\0') break;
    if (*s == '"')
    {
      char *dst = ++s;
      argv[argc++] = dst;
      while (*s && *s != '"')
      {
        if (*s == '\\' && (s[1] == '"' || s[1] == '\\')) s++;
        *dst++ = *s++;
      }
      if (*s == '"') s++;
      *dst = '\0';
    }
    else
    {
      argv[argc++] = s;
      while (*s && *s != ' ' && *s != '\t') s++;
      if (*s) *s++ = '\0';
    }
  }
  return argc;
}

