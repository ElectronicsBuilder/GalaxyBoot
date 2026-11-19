/*
 * galaxy_link.c - the BeaconGalaxy <-> AudioGalaxy serial protocol. See galaxy_link.h.
 */
#include "galaxy_link.h"
#include "cli_parse.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ARGS_NONE, ARGS_INT, ARGS_NAME_INT_INT, ARGS_SUB } ArgKind;

typedef struct {
  GxId        id;
  const char *verb;     /* first word */
  const char *sub;      /* second word for GALAXYAUDIO events */
  ArgKind     args;
  long        min, max; /* range of the first number */
} GxDef;

static const GxDef defs[] = {
  { GX_HELLO,        "GALAXYHELLO",       NULL, ARGS_NONE, 0, 0 },
  { GX_GETPLAYLIST,  "GALAXYGETPLAYLIST", NULL, ARGS_NONE, 0, 0 },
  { GX_PLAY,         "GALAXYPLAY",        NULL, ARGS_INT,  0, GX_PLAYLIST_MAX - 1 },
  { GX_PLAYTHEME,    "GALAXYPLAYTHEME",   NULL, ARGS_INT,  0, GX_PLAYLIST_MAX - 1 },
  { GX_VOL,          "GALAXYVOL",         NULL, ARGS_INT,  0, GX_VOLUME_MAX },
  { GX_RECALLVOL,    "GALAXYRECALLVOL",   NULL, ARGS_INT,  0, GX_VOLUME_MAX },
  { GX_STOP,         "GALAXYSTOP",        NULL, ARGS_NONE, 0, 0 },
  { GX_START,        "GALAXYSTART",       NULL, ARGS_NONE, 0, 0 },
  { GX_PAUSE,        "GALAXYPAUSE",       NULL, ARGS_NONE, 0, 0 },
  { GX_RESUME,       "GALAXYRESUME",      NULL, ARGS_NONE, 0, 0 },
  { GX_REPEATON,     "GALAXYREPEATON",    NULL, ARGS_NONE, 0, 0 },
  { GX_REPEATOFF,    "GALAXYREPEATOFF",   NULL, ARGS_NONE, 0, 0 },
  { GX_RECORD,       "GALAXYRECORD",      NULL, ARGS_NONE, 0, 0 },
  { GX_STOPRECORD,   "GALAXYSTOPRECORD",  NULL, ARGS_NONE, 0, 0 },
  { GX_READY,        "GALAXYREADY",       NULL, ARGS_INT,  0, 100000 },
  { GX_PLAYLIST,     "PLAYLIST",          NULL, ARGS_NAME_INT_INT, 0, GX_PLAYLIST_MAX - 1 },
  { GX_EV_WAVEEND,   "GALAXYAUDIO", "WAVEEND",   ARGS_SUB, 0, 0 },
  { GX_EV_RECALLVOL, "GALAXYAUDIO", "RECALLVOL", ARGS_SUB, 0, 0 },
  { GX_EV_REPEATSET, "GALAXYAUDIO", "REPEATSET", ARGS_SUB, 0, 0 },
  { GX_EV_PLAYING,   "GALAXYAUDIO", "PLAYING",   ARGS_SUB, 0, GX_PLAYLIST_MAX - 1 },
  { GX_EV_STOPPED,   "GALAXYAUDIO", "STOPPED",   ARGS_SUB, 0, 0 },
  { GX_EV_NOUSB,     "GALAXYAUDIO", "NOUSB",     ARGS_SUB, 0, 0 },
  { GX_EV_ERR,       "GALAXYAUDIO", "ERR",       ARGS_SUB, 0, 0 },
};

#define DEF_COUNT (sizeof defs / sizeof defs[0])

static int P_EqualNoCase(const char *a, const char *b)
{
  while (*a && *b)
  {
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
    a++;
    b++;
  }
  return *a == *b;
}

static const GxDef *P_Def(GxId id)
{
  for (size_t i = 0; i < DEF_COUNT; i++)
  {
    if (defs[i].id == id) return &defs[i];
  }
  return NULL;
}

const char *Gx_Name(GxId id)
{
  const GxDef *d = P_Def(id);
  if (d == NULL) return "?";
  return d->sub ? d->sub : d->verb;
}

static int P_Number(const char *s, long min, long max, long *out)
{
  char *end;
  if (s == NULL || *s == '\0') return -1;
  long v = strtol(s, &end, 10);
  if (*end != '\0' || v < min || v > max) return -1;
  *out = v;
  return 0;
}

static void P_CopyText(char *dst, const char *src)
{
  strncpy(dst, src, GX_NAME_MAX);
  dst[GX_NAME_MAX] = '\0';
}

GxParseResult Gx_Parse(char *line, GxMsg *out)
{
  char *argv[6];
  memset(out, 0, sizeof *out);
  int argc = Cli_Tokenize(line, argv, 6);
  if (argc == 0) return GX_PARSE_EMPTY;

  for (size_t i = 0; i < DEF_COUNT; i++)
  {
    const GxDef *d = &defs[i];
    if (!P_EqualNoCase(argv[0], d->verb)) continue;
    if (d->sub != NULL && (argc < 2 || !P_EqualNoCase(argv[1], d->sub))) continue;

    out->id = d->id;
    switch (d->args)
    {
      case ARGS_NONE:
        return GX_PARSE_OK;

      case ARGS_INT:
        if (argc < 2 || P_Number(argv[1], d->min, d->max, &out->a) != 0) return GX_PARSE_BAD_ARGS;
        return GX_PARSE_OK;

      case ARGS_NAME_INT_INT:   /* PLAYLIST "<name>" <index> <total> */
        if (argc < 4) return GX_PARSE_BAD_ARGS;
        P_CopyText(out->text, argv[1]);
        if (P_Number(argv[2], d->min, d->max, &out->a) != 0) return GX_PARSE_BAD_ARGS;
        if (P_Number(argv[3], 1, GX_PLAYLIST_MAX, &out->b) != 0) return GX_PARSE_BAD_ARGS;
        if (out->a >= out->b) return GX_PARSE_BAD_ARGS;
        return GX_PARSE_OK;

      case ARGS_SUB:
        if (d->id == GX_EV_PLAYING)
        {
          if (argc < 3 || P_Number(argv[2], d->min, d->max, &out->a) != 0) return GX_PARSE_BAD_ARGS;
        }
        else if (d->id == GX_EV_ERR && argc >= 3)
        {
          P_CopyText(out->text, argv[2]);
        }
        return GX_PARSE_OK;
    }
  }
  return GX_PARSE_UNKNOWN;
}

/* append a word, quoted when it has spaces, quotes or backslashes */
static size_t P_AppendWord(char *buf, size_t pos, size_t size, const char *w)
{
  int quote = (*w == '\0') || strpbrk(w, " \t\"\\") != NULL;
  if (quote && pos < size) buf[pos++] = '"';
  for (; *w && pos < size; w++)
  {
    if (*w == '"' || *w == '\\')
    {
      buf[pos++] = '\\';
      if (pos >= size) break;
    }
    buf[pos++] = *w;
  }
  if (quote && pos < size) buf[pos++] = '"';
  return pos;
}

size_t Gx_Format(const GxMsg *m, char *buf, size_t size)
{
  const GxDef *d = P_Def(m->id);
  if (d == NULL || size < 4u) return 0;
  int n;
  size_t pos;
  switch (d->args)
  {
    case ARGS_NONE:
      n = snprintf(buf, size, "%s", d->verb);
      break;
    case ARGS_INT:
      n = snprintf(buf, size, "%s %ld", d->verb, m->a);
      break;
    case ARGS_NAME_INT_INT:
      n = snprintf(buf, size, "%s ", d->verb);
      if (n < 0 || (size_t)n >= size) return 0;
      pos = P_AppendWord(buf, (size_t)n, size - 1u, m->text);
      buf[pos] = '\0';
      n = (int)pos;
      n += snprintf(buf + pos, size - pos, " %ld %ld", m->a, m->b);
      break;
    case ARGS_SUB:
      if (d->id == GX_EV_PLAYING) n = snprintf(buf, size, "%s %s %ld", d->verb, d->sub, m->a);
      else if (d->id == GX_EV_ERR)
      {
        n = snprintf(buf, size, "%s %s ", d->verb, d->sub);
        if (n < 0 || (size_t)n >= size) return 0;
        pos = P_AppendWord(buf, (size_t)n, size - 1u, m->text);
        buf[pos] = '\0';
        n = (int)pos;
      }
      else n = snprintf(buf, size, "%s %s", d->verb, d->sub);
      break;
    default:
      return 0;
  }
  if (n < 0 || (size_t)n + 2u >= size) return 0;
  buf[n++] = '\r';
  buf[n++] = '\n';
  buf[n] = '\0';
  return (size_t)n;
}

size_t Gx_FormatId(GxId id, long a, long b, const char *text, char *buf, size_t size)
{
  GxMsg m;
  memset(&m, 0, sizeof m);
  m.id = id;
  m.a = a;
  m.b = b;
  if (text != NULL) P_CopyText(m.text, text);
  return Gx_Format(&m, buf, size);
}

int Gx_LineFeed(GxLineAsm *la, char c)
{
  if (c == '\r' || c == '\n')
  {
    int ok = !la->overflow && la->len > 0;
    la->line[la->len] = '\0';
    la->len = 0;
    la->overflow = 0;
    return ok;
  }
  if (c == '\0') return 0;              /* padding from old senders */
  if (la->len >= GX_LINE_MAX)
  {
    la->overflow = 1;
    return 0;
  }
  la->line[la->len++] = c;
  return 0;
}
