/*
 * spectrum.c - see spectrum.h.
 *
 * Bands: Hann window, FFT, then the loudest bin of each band. The 16 bands
 * are spaced logarithmically (equal musical width) and each has at least one
 * bin of its own. Music has much less energy at the top than at the bottom,
 * so each band gets +3 dB per octave above 1 kHz (and less below): without it
 * the right half of the display would rarely move.
 */
#include "spectrum.h"
#include "fft.h"
#include <math.h>

#define FULL_SCALE   32767.0f
#define TILT_DB_OCT  3.0f

static float re[SPECTRUM_N] DSP_SCRATCH;   /* filled before every use */
static float im[SPECTRUM_N] DSP_SCRATCH;

static uint8_t P_Level(float db)
{
  float v = (db + SPECTRUM_RANGE_DB) * (255.0f / SPECTRUM_RANGE_DB);
  if (!(v > 0.0f)) return 0;            /* also NaN / -inf */
  if (v >= 255.0f) return 255;
  return (uint8_t)(v + 0.5f);
}

static float P_Db(float ratio)
{
  return 20.0f * log10f(ratio + 1e-9f);
}

static void P_ChannelLevels(const int16_t *s, uint32_t frames, SpectrumLevels *out)
{
  float sl = 0.0f, sr = 0.0f;
  for (uint32_t i = 0; i < frames; i++)
  {
    float l = s[2u * i], r = s[2u * i + 1u];
    sl += l * l;
    sr += r * r;
  }
  /* 0 dB = the RMS of a full-scale sine */
  const float ref = FULL_SCALE * 0.70710678f;
  out->left = frames ? P_Level(P_Db(sqrtf(sl / (float)frames) / ref)) : 0;
  out->right = frames ? P_Level(P_Db(sqrtf(sr / (float)frames) / ref)) : 0;
}

void Spectrum_Compute(const int16_t *stereo, uint32_t frames, uint32_t sample_rate, SpectrumLevels *out)
{
  for (unsigned b = 0; b < SPECTRUM_BANDS; b++) out->band[b] = 0;
  P_ChannelLevels(stereo, frames, out);

  const uint32_t step = sample_rate > 32000u ? 2u : 1u;      /* frames per FFT point */
  if (sample_rate == 0u || frames < SPECTRUM_N * step) return;
  const float rate = (float)sample_rate / (float)step;

  /* mono, from the middle of the block, Hann window */
  const int16_t *s = stereo + 2u * ((frames - SPECTRUM_N * step) / 2u);
  for (unsigned i = 0; i < SPECTRUM_N; i++)
  {
    float sum = 0.0f;
    for (uint32_t k = 0; k < step; k++)
    {
      const int16_t *f = s + 2u * (i * step + k);
      sum += (float)f[0] + (float)f[1];
    }
    float w = 0.5f - 0.5f * cosf(6.28318530718f * (float)i / (float)(SPECTRUM_N - 1));
    re[i] = sum / (float)(2u * step) * w;
    im[i] = 0.0f;
  }
  (void)Fft_Run(re, im, SPECTRUM_N);
  Fft_Magnitude(re, im, re, SPECTRUM_N);

  /* a full-scale sine peaks at A * N / 4 with the Hann window */
  const float ref = FULL_SCALE * (float)SPECTRUM_N / 4.0f;
  const float bin_hz = rate / (float)SPECTRUM_N;
  float hi_hz = rate / 2.0f;
  if (hi_hz > SPECTRUM_HI_HZ) hi_hz = SPECTRUM_HI_HZ;
  const float ratio = powf(hi_hz / SPECTRUM_LO_HZ, 1.0f / (float)SPECTRUM_BANDS);

  unsigned lo = 1;                       /* bin 0 is DC */
  float edge = SPECTRUM_LO_HZ;
  for (unsigned b = 0; b < SPECTRUM_BANDS && lo < SPECTRUM_N / 2u; b++)
  {
    float next = edge * ratio;
    unsigned hi = (unsigned)(next / bin_hz + 0.5f);
    if (hi <= lo) hi = lo + 1u;
    if (hi > SPECTRUM_N / 2u) hi = SPECTRUM_N / 2u;
    float peak = 0.0f;
    for (unsigned k = lo; k < hi; k++)
    {
      if (re[k] > peak) peak = re[k];
    }
    float centre = sqrtf(edge * next);
    out->band[b] = P_Level(P_Db(peak / ref) + TILT_DB_OCT * log2f(centre / 1000.0f));
    lo = hi;
    edge = next;
  }
}

static const char HEX[] = "0123456789ABCDEF";

size_t Spectrum_Encode(const SpectrumLevels *l, char *buf, size_t size)
{
  if (size < SPECTRUM_HEX_LEN + 1u) return 0;
  size_t n = 0;
  for (unsigned i = 0; i < SPECTRUM_BANDS + 2u; i++)
  {
    uint8_t v = i < SPECTRUM_BANDS ? l->band[i] : (i == SPECTRUM_BANDS ? l->left : l->right);
    buf[n++] = HEX[v >> 4];
    buf[n++] = HEX[v & 15u];
  }
  buf[n] = '\0';
  return n;
}

static int P_Nibble(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

int Spectrum_Decode(const char *hex, SpectrumLevels *out)
{
  uint8_t v[SPECTRUM_BANDS + 2];
  for (unsigned i = 0; i < SPECTRUM_BANDS + 2u; i++)
  {
    int h = P_Nibble(hex[2u * i]);
    if (h < 0) return -1;
    int l = P_Nibble(hex[2u * i + 1u]);
    if (l < 0) return -1;
    v[i] = (uint8_t)(h * 16 + l);
  }
  if (hex[SPECTRUM_HEX_LEN] != '\0') return -1;
  for (unsigned i = 0; i < SPECTRUM_BANDS; i++) out->band[i] = v[i];
  out->left = v[SPECTRUM_BANDS];
  out->right = v[SPECTRUM_BANDS + 1];
  return 0;
}
