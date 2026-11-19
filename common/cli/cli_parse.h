/*
 * cli_parse.h - command line tokenizer.
 */
#ifndef COMMON_CLI_PARSE_H
#define COMMON_CLI_PARSE_H

/* Split s in place into words. "double quoted" words may contain spaces and
 * the escapes \" and \. Returns the number of words (at most max_args). */
int Cli_Tokenize(char *s, char **argv, int max_args);

#endif
