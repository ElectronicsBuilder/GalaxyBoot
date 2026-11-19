/*
 * Host unit tests for the hardware-independent code of the Galaxy projects.
 * Build and run: sh tests/host/run_tests.sh (uses the PC's gcc).
 * The shared code is always tested; TEST_AUDIO / TEST_BEACON / TEST_BOOT add
 * the tests of each project present (run_tests.sh defines them).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cli_parse.h"
#include "crc16.h"
#include "crc32.h"
#include "galaxy_link.h"
#include "ring.h"
#ifdef TEST_AUDIO
#include "wav.h"
#include "playlist.h"
#endif
#ifdef TEST_BEACON
#include "gif_names.h"
#include "bmp.h"
#include "matrix.h"
#include "fft.h"
#include <math.h>
#endif
#ifdef TEST_BOOT
#include "image.h"
#include "boot_jump.h"
#endif

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
  printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---------- crc ---------- */
static void test_crc(void)
{
  CHECK(Crc16("123456789", 9) == 0x29B1);
  CHECK(Crc32("123456789", 9) == 0xCBF43926u);
  uint32_t c = Crc32_Update(0, "1234", 4);
  c = Crc32_Update(c, "56789", 5);
  CHECK(c == 0xCBF43926u);
  CHECK(Crc32("", 0) == 0);
}

/* ---------- ring ---------- */
static void test_ring(void)
{
  uint8_t mem[8], b = 0;
  Ring r;
  Ring_Init(&r, mem, sizeof mem);
  CHECK(Ring_Empty(&r));
  for (int i = 0; i < 7; i++) CHECK(Ring_Put(&r, (uint8_t)i));
  CHECK(!Ring_Put(&r, 99));            /* one slot stays free */
  CHECK(Ring_Count(&r) == 7);
  CHECK(Ring_Get(&r, &b) && b == 0);
  CHECK(Ring_Put(&r, 7));
  for (int i = 1; i <= 7; i++) CHECK(Ring_Get(&r, &b) && b == i);
  CHECK(!Ring_Get(&r, &b));
}

/* ---------- galaxy link ---------- */
static GxParseResult parse(const char *text, GxMsg *m)
{
  static char line[256];
  strcpy(line, text);
  return Gx_Parse(line, m);
}

static void test_link_parse(void)
{
  GxMsg m;
  CHECK(parse("", &m) == GX_PARSE_EMPTY);
  CHECK(parse("   ", &m) == GX_PARSE_EMPTY);
  CHECK(parse("GALAXYPLAY 3", &m) == GX_PARSE_OK && m.id == GX_PLAY && m.a == 3);
  CHECK(parse("galaxyplay 3", &m) == GX_PARSE_OK && m.id == GX_PLAY);
  CHECK(parse("GALAXYPLAY", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("GALAXYPLAY x", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("GALAXYPLAY 100", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("GALAXYPLAY -1", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("GALAXYVOL 100", &m) == GX_PARSE_OK && m.a == 100);
  CHECK(parse("GALAXYVOL 101", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("GALAXYVOL  42 ", &m) == GX_PARSE_OK && m.id == GX_VOL && m.a == 42);
  CHECK(parse("GALAXYSTOP", &m) == GX_PARSE_OK && m.id == GX_STOP);
  CHECK(parse("GALAXYSTOPRECORD", &m) == GX_PARSE_OK && m.id == GX_STOPRECORD);
  CHECK(parse("GALAXYREPEATON", &m) == GX_PARSE_OK && m.id == GX_REPEATON);
  CHECK(parse("OLDPLAY 3", &m) == GX_PARSE_UNKNOWN);
  CHECK(parse("GALAXYREADY 27", &m) == GX_PARSE_OK && m.id == GX_READY && m.a == 27);
  CHECK(parse("PLAYLIST \"my song.wav\" 4 27", &m) == GX_PARSE_OK && m.id == GX_PLAYLIST &&
        strcmp(m.text, "my song.wav") == 0 && m.a == 4 && m.b == 27);
  CHECK(parse("PLAYLIST tetris.wav 26 27", &m) == GX_PARSE_OK && strcmp(m.text, "tetris.wav") == 0);
  CHECK(parse("PLAYLIST tetris.wav 27 27", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("PLAYLIST tetris.wav", &m) == GX_PARSE_BAD_ARGS);
  CHECK(parse("GALAXYAUDIO WAVEEND", &m) == GX_PARSE_OK && m.id == GX_EV_WAVEEND);
  CHECK(parse("GALAXYAUDIO PLAYING 5", &m) == GX_PARSE_OK && m.id == GX_EV_PLAYING && m.a == 5);
  CHECK(parse("GALAXYAUDIO ERR \"cannot open file\"", &m) == GX_PARSE_OK && m.id == GX_EV_ERR &&
        strcmp(m.text, "cannot open file") == 0);
  CHECK(parse("GALAXYAUDIO BOGUS", &m) == GX_PARSE_UNKNOWN);
}

static void test_link_format(void)
{
  char buf[GX_LINE_MAX + 4];
  GxMsg m;
  CHECK(Gx_FormatId(GX_PLAY, 7, 0, NULL, buf, sizeof buf) > 0 && strcmp(buf, "GALAXYPLAY 7\r\n") == 0);
  CHECK(Gx_FormatId(GX_STOP, 0, 0, NULL, buf, sizeof buf) > 0 && strcmp(buf, "GALAXYSTOP\r\n") == 0);
  CHECK(Gx_FormatId(GX_EV_WAVEEND, 0, 0, NULL, buf, sizeof buf) > 0 &&
        strcmp(buf, "GALAXYAUDIO WAVEEND\r\n") == 0);
  CHECK(Gx_FormatId(GX_PLAYLIST, 2, 27, "desert.wav", buf, sizeof buf) > 0 &&
        strcmp(buf, "PLAYLIST desert.wav 2 27\r\n") == 0);
  CHECK(Gx_FormatId(GX_PLAYLIST, 3, 27, "my \"best\" song.wav", buf, sizeof buf) > 0 &&
        strcmp(buf, "PLAYLIST \"my \\\"best\\\" song.wav\" 3 27\r\n") == 0);

  /* round trip through the parser */
  buf[strlen(buf) - 2] = '\0';
  CHECK(Gx_Parse(buf, &m) == GX_PARSE_OK && strcmp(m.text, "my \"best\" song.wav") == 0 && m.a == 3);
  CHECK(Gx_FormatId(GX_EV_ERR, 0, 0, "no such track", buf, sizeof buf) > 0 &&
        strcmp(buf, "GALAXYAUDIO ERR \"no such track\"\r\n") == 0);
  CHECK(Gx_FormatId(GX_PLAY, 7, 0, NULL, buf, 8) == 0);   /* does not fit */
  CHECK(strcmp(Gx_Name(GX_EV_WAVEEND), "WAVEEND") == 0);
}

static void test_link_lines(void)
{
  GxLineAsm la;
  memset(&la, 0, sizeof la);
  const char *in = "\rGALAXYSTOP\r\nGALAXYPLAY 2\n\0\0X";
  int lines = 0;
  for (size_t i = 0; i < 27; i++)
  {
    if (Gx_LineFeed(&la, in[i]))
    {
      lines++;
      if (lines == 1) CHECK(strcmp(la.line, "GALAXYSTOP") == 0);
      if (lines == 2) CHECK(strcmp(la.line, "GALAXYPLAY 2") == 0);
    }
  }
  CHECK(lines == 2);
  /* an over-long line is dropped, the next one is fine */
  memset(&la, 0, sizeof la);
  for (int i = 0; i < GX_LINE_MAX + 10; i++) CHECK(!Gx_LineFeed(&la, 'A'));
  CHECK(!Gx_LineFeed(&la, '\r'));
  for (const char *p = "GALAXYSTART"; *p; p++) Gx_LineFeed(&la, *p);
  CHECK(Gx_LineFeed(&la, '\n') && strcmp(la.line, "GALAXYSTART") == 0);
}

#ifdef TEST_AUDIO
/* ---------- wav ---------- */
typedef struct { const uint8_t *data; size_t size; } Mem;

static size_t mem_read(void *ctx, uint32_t off, void *buf, size_t len)
{
  Mem *m = (Mem *)ctx;
  if (off >= m->size) return 0;
  if (len > m->size - off) len = m->size - off;
  memcpy(buf, m->data + off, len);
  return len;
}

static size_t put_chunk(uint8_t *p, const char *id, const void *body, uint32_t size)
{
  memcpy(p, id, 4);
  p[4] = (uint8_t)size; p[5] = (uint8_t)(size >> 8); p[6] = (uint8_t)(size >> 16); p[7] = (uint8_t)(size >> 24);
  if (body) memcpy(p + 8, body, size);
  return 8u + size + (size & 1u);
}

static void test_wav(void)
{
  uint8_t f[512];
  WavInfo w;
  Mem m = { f, 0 };

  /* canonical header from Wav_MakeHeader */
  Wav_MakeHeader(f, 16000, 1, 16, 100);
  m.size = 44 + 100;
  CHECK(Wav_Parse(mem_read, &m, (uint32_t)m.size, &w) == WAV_OK);
  CHECK(w.channels == 1 && w.sample_rate == 16000 && w.bits == 16 && w.data_offset == 44 && w.data_size == 100);

  /* LIST chunk (odd size, padded) before fmt, data after: like many files on the stick */
  size_t p = 0;
  memcpy(f, "RIFF\0\0\0\0WAVE", 12);
  p = 12;
  p += put_chunk(f + p, "LIST", "INFOabc", 7);
  uint8_t fmt[16] = { 1, 0, 2, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 2, 0, 4, 0, 16, 0 };
  p += put_chunk(f + p, "fmt ", fmt, 16);
  size_t data_at = p + 8;
  p += put_chunk(f + p, "data", NULL, 64);
  m.size = p;
  CHECK(Wav_Parse(mem_read, &m, (uint32_t)m.size, &w) == WAV_OK);
  CHECK(w.channels == 2 && w.sample_rate == 44100 && w.data_offset == data_at && w.data_size == 64);

  /* data size larger than the file (truncated copy): clipped to what is there */
  m.size = data_at + 30;
  CHECK(Wav_Parse(mem_read, &m, (uint32_t)m.size, &w) == WAV_OK && w.data_size == 28);

  /* unsupported: 24-bit */
  Wav_MakeHeader(f, 44100, 2, 24, 60);
  m.size = 104;
  CHECK(Wav_Parse(mem_read, &m, (uint32_t)m.size, &w) == WAV_ERR_UNSUPPORTED);
  /* not RIFF */
  memcpy(f, "RIFX", 4);
  CHECK(Wav_Parse(mem_read, &m, (uint32_t)m.size, &w) == WAV_ERR_NOT_RIFF);
  /* data before fmt */
  memcpy(f, "RIFF\0\0\0\0WAVE", 12);
  p = 12 + put_chunk(f + 12, "data", NULL, 8);
  m.size = p;
  CHECK(Wav_Parse(mem_read, &m, (uint32_t)m.size, &w) == WAV_ERR_NO_FMT);
}

static void test_wav_convert(void)
{
  int16_t buf[16] = { 100, -200, 300, 0 };
  size_t n = Wav_ToStereo16((uint8_t *)buf, 6, 1, 16);    /* 3 mono samples */
  CHECK(n == 12);
  CHECK(buf[0] == 100 && buf[1] == 100 && buf[2] == -200 && buf[3] == -200 && buf[4] == 300 && buf[5] == 300);

  uint8_t b8[32] = { 128, 255, 0 };
  n = Wav_ToStereo16(b8, 3, 1, 8);
  int16_t *s = (int16_t *)(void *)b8;
  CHECK(n == 12 && s[0] == 0 && s[1] == 0 && s[2] == 127 * 256 && s[4] == -128 * 256 && s[5] == -128 * 256);

  uint8_t st8[32] = { 0, 255 };
  n = Wav_ToStereo16(st8, 2, 2, 8);
  s = (int16_t *)(void *)st8;
  CHECK(n == 4 && s[0] == -128 * 256 && s[1] == 127 * 256);

  int16_t st16[4] = { 1, 2, 3, 4 };
  CHECK(Wav_ToStereo16((uint8_t *)st16, 8, 2, 16) == 8 && st16[2] == 3);
}

/* ---------- playlist ---------- */
static void test_playlist(void)
{
  CHECK(Playlist_IsWavName("tetris.wav"));
  CHECK(Playlist_IsWavName("FUR-ELISE.WAV"));
  CHECK(!Playlist_IsWavName("._tetris.wav"));
  CHECK(!Playlist_IsWavName("notes.txt"));
  CHECK(!Playlist_IsWavName(".wav"));
  CHECK(!Playlist_IsWavName("wav"));

  char names[4][GX_NAME_MAX + 1] = { "tetris.wav", "Alla-turca.wav", "desert.wav", "ChillingMusic.wav" };
  Playlist_SortNames(names, 4);
  CHECK(strcmp(names[0], "Alla-turca.wav") == 0);
  CHECK(strcmp(names[1], "ChillingMusic.wav") == 0);
  CHECK(strcmp(names[2], "desert.wav") == 0);
  CHECK(strcmp(names[3], "tetris.wav") == 0);
}

#endif /* TEST_AUDIO */

#ifdef TEST_BEACON
/* ---------- BeaconGalaxy: GIF names ---------- */
static void test_gif_names(void)
{
  char g[GIF_NAME_MAX + 1];
  unsigned f = 99;
  CHECK(GifName_Parse("BAT-12.bmp", g, sizeof g, &f) && strcmp(g, "BAT") == 0 && f == 12);
  CHECK(GifName_Parse("circle2-0.BMP", g, sizeof g, &f) && strcmp(g, "circle2") == 0 && f == 0);
  CHECK(GifName_Parse("A-B-3.bmp", g, sizeof g, &f) && strcmp(g, "A-B") == 0 && f == 3);
  CHECK(!GifName_Parse("BAT.bmp", g, sizeof g, &f));
  CHECK(!GifName_Parse("BAT-.bmp", g, sizeof g, &f));
  CHECK(!GifName_Parse("BAT-1x.bmp", g, sizeof g, &f));
  CHECK(!GifName_Parse("BAT-1.wav", g, sizeof g, &f));
  CHECK(!GifName_Parse("-1.bmp", g, sizeof g, &f));

  static GifTable t;
  GifTable_Clear(&t);
  CHECK(GifTable_Add(&t, "ZED-0.bmp"));
  CHECK(GifTable_Add(&t, "bat-4.bmp"));
  CHECK(GifTable_Add(&t, "BAT-1.bmp"));
  CHECK(!GifTable_Add(&t, "notes.txt"));
  GifTable_Sort(&t);
  CHECK(t.count == 2);
  CHECK(strcasecmp(t.gif[0].name, "BAT") == 0 && t.gif[0].frames == 5);
  CHECK(strcmp(t.gif[1].name, "ZED") == 0 && t.gif[1].frames == 1);
}

/* ---------- BeaconGalaxy: BMP frames ---------- */
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static size_t make_bmp(uint8_t *b, int w, int h, unsigned bpp, unsigned comp)
{
  memset(b, 0, 138);
  b[0] = 'B'; b[1] = 'M';
  put32(b + 10, 138);            /* data offset, as the converter writes */
  put32(b + 14, 124);            /* BITMAPV5HEADER */
  put32(b + 18, (uint32_t)w);
  put32(b + 22, (uint32_t)h);
  b[26] = 1;
  b[28] = (uint8_t)bpp;
  put32(b + 30, comp);
  put32(b + 54, 0x00FF0000u); put32(b + 58, 0x0000FF00u); put32(b + 62, 0x000000FFu);
  return 138;
}

static void test_bmp(void)
{
  uint8_t h[138];
  BmpInfo bi;
  make_bmp(h, 22, 22, 24, 0);
  CHECK(Bmp_Parse(h, sizeof h, &bi) == 0);
  CHECK(bi.stride == 68 && bi.data_offset == 138 && !bi.top_down && bi.height == 22);
  CHECK(Bmp_RowOffset(&bi, 0) == 138 + 21 * 68);    /* top row is stored last */
  CHECK(Bmp_RowOffset(&bi, 21) == 138);
  uint8_t row24[6] = { 1, 2, 3, 4, 5, 6 };
  uint8_t r, g, b;
  Bmp_Pixel(&bi, row24, 1, &r, &g, &b);
  CHECK(r == 6 && g == 5 && b == 4);

  make_bmp(h, 22, -22, 32, 3);
  CHECK(Bmp_Parse(h, sizeof h, &bi) == 0 && bi.top_down && bi.stride == 88);
  CHECK(Bmp_RowOffset(&bi, 0) == 138);
  uint8_t px[4];
  put32(px, 0xFF102030u);
  Bmp_Pixel(&bi, px, 0, &r, &g, &b);
  CHECK(r == 0x10 && g == 0x20 && b == 0x30);

  make_bmp(h, 22, 22, 32, 3);
  put32(h + 54, 0x000000FFu); put32(h + 62, 0x00FF0000u);   /* red and blue swapped */
  CHECK(Bmp_Parse(h, sizeof h, &bi) == 0);
  Bmp_Pixel(&bi, px, 0, &r, &g, &b);
  CHECK(r == 0x30 && b == 0x10);

  make_bmp(h, 22, 22, 16, 0);
  CHECK(Bmp_Parse(h, sizeof h, &bi) == -2);
  make_bmp(h, 22, 22, 32, 1);
  CHECK(Bmp_Parse(h, sizeof h, &bi) == -2);         /* RLE */
  make_bmp(h, 0, 22, 24, 0);
  CHECK(Bmp_Parse(h, sizeof h, &bi) == -3);
  make_bmp(h, BMP_MAX_WIDTH + 1, 22, 24, 0);
  CHECK(Bmp_Parse(h, sizeof h, &bi) == -3);
  h[0] = 'X';
  CHECK(Bmp_Parse(h, sizeof h, &bi) == -1);
  CHECK(Bmp_Parse(h, 20, &bi) == -1);

  CHECK(Bmp_SourceIndex(5, 22, 22) == 5);
  CHECK(Bmp_SourceIndex(0, 22, 64) == 1);
  CHECK(Bmp_SourceIndex(21, 22, 64) == 62);
  for (int d = 0; d < 22; d++) { int s = Bmp_SourceIndex(d, 22, 64); CHECK(s >= 0 && s < 64); }
}

/* ---------- BeaconGalaxy: matrix layout and colour ---------- */
static void test_matrix(void)
{
  CHECK(Matrix_ChainIndex(0, 0) == 0);
  CHECK(Matrix_ChainIndex(21, 0) == 21);
  CHECK(Matrix_ChainIndex(21, 1) == 22);      /* odd rows run right to left */
  CHECK(Matrix_ChainIndex(0, 1) == 43);
  CHECK(Matrix_ChainIndex(21, 21) == 22 * 21);
  CHECK(Matrix_ChainIndex(22, 0) == -1 && Matrix_ChainIndex(0, -1) == -1);
  static int seen[MX_LEDS];
  int ok = 1;
  for (int y = 0; y < MX_H; y++)
    for (int x = 0; x < MX_W; x++) { int i = Matrix_ChainIndex(x, y); if (i < 0 || i >= MX_LEDS || seen[i]++) ok = 0; }
  CHECK(ok);
  CHECK(Matrix_Gamma(0) == 0 && Matrix_Gamma(255) == 255 && Matrix_Gamma(128) < 64);
  CHECK(Matrix_Scale(255, 255) == 255 && Matrix_Scale(255, 0) == 0 && Matrix_Scale(200, 128) == 100);
  Rgb c = Hsv_ToRgb(0, 255, 255);
  CHECK(c.r == 255 && c.g < 10 && c.b == 0);
  c = Hsv_ToRgb(85, 255, 255);
  CHECK(c.g == 255 && c.r < 10);
  c = Hsv_ToRgb(170, 255, 255);
  CHECK(c.b == 255 && c.g < 10);
  c = Hsv_ToRgb(99, 0, 77);
  CHECK(c.r == 77 && c.g == 77 && c.b == 77);
}

/* ---------- BeaconGalaxy: FFT ---------- */
static void test_fft(void)
{
  static float re[256], im[256], mag[128];
  for (int i = 0; i < 256; i++) { re[i] = cosf(6.2831853f * 10.0f * (float)i / 256.0f); im[i] = 0; }
  CHECK(Fft_Run(re, im, 256) == 0);
  Fft_Magnitude(re, im, mag, 256);
  int peak = 0;
  for (int k = 1; k < 128; k++) if (mag[k] > mag[peak]) peak = k;
  CHECK(peak == 10);
  CHECK(fabsf(mag[10] - 128.0f) < 0.5f);
  CHECK(mag[3] < 0.01f && mag[50] < 0.01f);
  for (int i = 0; i < 8; i++) { re[i] = 1.0f; im[i] = 0; }
  CHECK(Fft_Run(re, im, 8) == 0 && fabsf(re[0] - 8.0f) < 1e-4f && fabsf(re[1]) < 1e-4f);
  CHECK(Fft_Run(re, im, 12) == -1 && Fft_Run(re, im, 512) == -1);
}

#endif /* TEST_BEACON */

#ifdef TEST_BOOT
/* ---------- GalaxyBoot: update image ---------- */
static void put_le(uint8_t *p, uint32_t v)
{
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static void test_image(void)
{
  uint8_t t[IMAGE_TRAILER_SIZE] = { 0 };
  ImageInfo info;
  put_le(t, IMAGE_MAGIC);
  put_le(t + 4, 1000);
  put_le(t + 8, 0x12345678u);
  CHECK(Image_CheckTrailer(t, 1016, BOOT_APP_MAX_SIZE, &info) == IMAGE_OK && info.length == 1000 &&
        info.crc32 == 0x12345678u);
  CHECK(Image_CheckTrailer(t, 1017, BOOT_APP_MAX_SIZE, &info) == IMAGE_BAD_LENGTH);
  CHECK(Image_CheckTrailer(t, 20, BOOT_APP_MAX_SIZE, &info) == IMAGE_TOO_SMALL);
  CHECK(Image_CheckTrailer(t, BOOT_APP_MAX_SIZE + IMAGE_TRAILER_SIZE + 4, BOOT_APP_MAX_SIZE, &info) == IMAGE_TOO_BIG);
  put_le(t, 0x4D495858u);
  CHECK(Image_CheckTrailer(t, 1016, BOOT_APP_MAX_SIZE, &info) == IMAGE_NO_TRAILER);

  uint8_t v[8];
  put_le(v, 0x20020000u);
  put_le(v + 4, 0x08040201u);
  CHECK(Image_CheckVectors(v) == IMAGE_OK);
  put_le(v + 4, 0x08000201u);                          /* reset vector inside the bootloader */
  CHECK(Image_CheckVectors(v) == IMAGE_BAD_VECTORS);
  put_le(v, 0xFFFFFFFFu);
  put_le(v + 4, 0xFFFFFFFFu);                          /* erased flash */
  CHECK(Image_CheckVectors(v) == IMAGE_BAD_VECTORS);
  CHECK(strcmp(Image_ResultText(IMAGE_BAD_CRC), "BAD CRC") == 0);
}
#endif /* TEST_BOOT */

int main(void)
{
  test_crc();
  test_ring();
  test_link_parse();
  test_link_format();
  test_link_lines();
#ifdef TEST_AUDIO
  test_wav();
  test_wav_convert();
  test_playlist();
#endif
#ifdef TEST_BEACON
  test_gif_names();
  test_bmp();
  test_matrix();
  test_fft();
#endif
#ifdef TEST_BOOT
  test_image();
#endif
  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
