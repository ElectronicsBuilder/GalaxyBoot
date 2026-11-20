/*
 * fft.c - see fft.h. Twiddles come from a quarter-wave table built once.
 */
#include "fft.h"
#include <math.h>

static float sine[FFT_MAX_N] DSP_SCRATCH;        /* sin(2*pi*i/FFT_MAX_N) */
static int ready;

static void P_Table(void)
{
  for (unsigned i = 0; i < FFT_MAX_N; i++) sine[i] = sinf(6.28318530718f * (float)i / (float)FFT_MAX_N);
  ready = 1;
}

int Fft_Run(float *re, float *im, unsigned n)
{
  if (n < 2u || n > FFT_MAX_N || (n & (n - 1u)) != 0) return -1;
  if (!ready) P_Table();

  /* bit reversal */
  for (unsigned i = 1, j = 0; i < n; i++)
  {
    unsigned bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j |= bit;
    if (i < j)
    {
      float t = re[i]; re[i] = re[j]; re[j] = t;
      t = im[i]; im[i] = im[j]; im[j] = t;
    }
  }
  /* butterflies */
  for (unsigned len = 2; len <= n; len <<= 1)
  {
    unsigned step = FFT_MAX_N / len;
    unsigned half = len >> 1;
    for (unsigned i = 0; i < n; i += len)
    {
      for (unsigned k = 0; k < half; k++)
      {
        unsigned a = k * step;
        float wr = sine[(a + FFT_MAX_N / 4u) % FFT_MAX_N];   /* cos */
        float wi = -sine[a];                                 /* -sin: forward transform */
        unsigned p = i + k, q = p + half;
        float tr = re[q] * wr - im[q] * wi;
        float ti = re[q] * wi + im[q] * wr;
        re[q] = re[p] - tr; im[q] = im[p] - ti;
        re[p] += tr;        im[p] += ti;
      }
    }
  }
  return 0;
}

void Fft_Magnitude(const float *re, const float *im, float *mag, unsigned n)
{
  for (unsigned k = 0; k < n / 2u; k++) mag[k] = sqrtf(re[k] * re[k] + im[k] * im[k]);
}
