/*
 * spectrum.h - what the music sounds like, in a few bytes: 16 frequency
 * bands and the left / right level of a block of 16-bit stereo samples.
 *
 * AudioGalaxy computes it from the samples it sends to the codec and passes
 * it to BeaconGalaxy in GALAXYAUDIO LEVELS (docs/protocol.md), which draws it
 * on the OLED. Pure code, host tested.
 *
 * Levels are 0..255 over SPECTRUM_RANGE_DB decibels below full scale: 0 is
 * silence (or quieter than the range), 255 a full-scale sine.
 */
#ifndef COMMON_SPECTRUM_H
#define COMMON_SPECTRUM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPECTRUM_BANDS     16
#define SPECTRUM_N         256      /* FFT points */
#define SPECTRUM_LO_HZ     60.0f    /* lower edge of the first band */
#define SPECTRUM_HI_HZ     16000.0f /* upper edge of the last band (or the Nyquist limit) */
#define SPECTRUM_RANGE_DB  60.0f
#define SPECTRUM_HEX_LEN   (2 * (SPECTRUM_BANDS + 2))   /* encoded length */

typedef struct {
  uint8_t band[SPECTRUM_BANDS];   /* low to high frequency */
  uint8_t left, right;            /* RMS level of each channel */
} SpectrumLevels;

/* Levels of interleaved stereo samples (L R L R ...). The bands look at
 * SPECTRUM_N points from the middle of the block (two frames averaged per point
 * above 32 kHz, for a finer low end); the channel levels use every frame. */
void Spectrum_Compute(const int16_t *stereo, uint32_t frames, uint32_t sample_rate, SpectrumLevels *out);

/* As hex, bands first then left and right: SPECTRUM_HEX_LEN characters + NUL.
 * Returns the length, 0 if buf is too small. */
size_t Spectrum_Encode(const SpectrumLevels *l, char *buf, size_t size);
/* Back from hex; returns 0, or -1 if the text is not exactly that. */
int Spectrum_Decode(const char *hex, SpectrumLevels *out);

#ifdef __cplusplus
}
#endif

#endif
