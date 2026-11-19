/*
 * galaxy_link.h - the BeaconGalaxy <-> AudioGalaxy serial protocol.
 *
 * USART2, 115200 8N1, ASCII lines. A line ends with CR or LF (CR LF counts
 * once); empty lines are ignored. Words are separated by spaces; a word with
 * spaces is written in "double quotes" (\" and \\ escapes). Verbs are not
 * case sensitive. Full description: docs/protocol.md.
 *
 * BeaconGalaxy -> AudioGalaxy (commands):
 *   GALAXYHELLO                  ask for GALAXYREADY (link check)
 *   GALAXYGETPLAYLIST            send the playlist
 *   GALAXYPLAY <index>           play track <index> (0-based)
 *   GALAXYPLAYTHEME <index>      play track <index> as theme music
 *   GALAXYVOL <0..100>           set the volume now
 *   GALAXYRECALLVOL <0..100>     volume restored from the settings at boot
 *   GALAXYSTOP / GALAXYSTART     stop / start playing
 *   GALAXYPAUSE / GALAXYRESUME   pause / resume
 *   GALAXYREPEATON / GALAXYREPEATOFF   repeat the current track or not
 *   GALAXYRECORD / GALAXYSTOPRECORD    record rec.wav from the microphone / stop
 *
 * AudioGalaxy -> BeaconGalaxy (events and replies):
 *   GALAXYREADY <tracks>         at boot, on USB mount and in reply to GALAXYHELLO
 *   PLAYLIST "<name>" <index> <total>   one line per track (reply to GALAXYGETPLAYLIST)
 *   GALAXYAUDIO WAVEEND          the current track ended
 *   GALAXYAUDIO RECALLVOL        GALAXYRECALLVOL applied
 *   GALAXYAUDIO REPEATSET        GALAXYREPEATON/OFF applied
 *   GALAXYAUDIO PLAYING <index>  a track started
 *   GALAXYAUDIO STOPPED          playing stopped
 *   GALAXYAUDIO NOUSB            no USB stick / no tracks
 *   GALAXYAUDIO ERR <reason>     a command could not be done
 *
 * This module has no hardware access (tested on the PC).
 */
#ifndef COMMON_GALAXY_LINK_H
#define COMMON_GALAXY_LINK_H

#include <stddef.h>
#include <stdint.h>

#define GX_LINE_MAX      128   /* longest line, terminator excluded */
#define GX_NAME_MAX      64    /* longest track name kept */
#define GX_PLAYLIST_MAX  100   /* tracks listed */
#define GX_VOLUME_MAX    100

typedef enum {
  GX_NONE = 0,
  /* commands (BeaconGalaxy -> AudioGalaxy) */
  GX_HELLO,
  GX_GETPLAYLIST,
  GX_PLAY,          /* a = index */
  GX_PLAYTHEME,     /* a = index */
  GX_VOL,           /* a = volume */
  GX_RECALLVOL,     /* a = volume */
  GX_STOP,
  GX_START,
  GX_PAUSE,
  GX_RESUME,
  GX_REPEATON,
  GX_REPEATOFF,
  GX_RECORD,
  GX_STOPRECORD,
  /* events (AudioGalaxy -> BeaconGalaxy) */
  GX_READY,         /* a = number of tracks */
  GX_PLAYLIST,      /* text = name, a = index, b = total */
  GX_EV_WAVEEND,
  GX_EV_RECALLVOL,
  GX_EV_REPEATSET,
  GX_EV_PLAYING,    /* a = index */
  GX_EV_STOPPED,
  GX_EV_NOUSB,
  GX_EV_ERR,        /* text = reason */
  GX_COUNT
} GxId;

typedef struct {
  GxId id;
  long a;
  long b;
  char text[GX_NAME_MAX + 1];
} GxMsg;

typedef enum {
  GX_PARSE_OK = 0,
  GX_PARSE_EMPTY,        /* blank line: ignore */
  GX_PARSE_UNKNOWN,      /* verb not known */
  GX_PARSE_BAD_ARGS,     /* missing / out of range argument */
} GxParseResult;

/* Parse one line (without terminator). The line is modified. */
GxParseResult Gx_Parse(char *line, GxMsg *out);

/* Format a message as one line ending in "\r\n"; returns its length
 * (0 if it does not fit). */
size_t Gx_Format(const GxMsg *msg, char *buf, size_t size);

/* Convenience: format a message from its parts */
size_t Gx_FormatId(GxId id, long a, long b, const char *text, char *buf, size_t size);

/* Name of a message for logs, e.g. "GALAXYPLAY" */
const char *Gx_Name(GxId id);

/* Line assembler: feed received bytes; returns 1 when a full line is in
 * la->line (NUL terminated). Over-long lines are dropped. */
typedef struct {
  char   line[GX_LINE_MAX + 1];
  size_t len;
  int    overflow;
} GxLineAsm;

int Gx_LineFeed(GxLineAsm *la, char c);

#endif
