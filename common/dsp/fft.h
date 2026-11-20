/*
 * fft.h - small in-place radix-2 FFT (float, pure, host tested).
 */
#ifndef COMMON_FFT_H
#define COMMON_FFT_H

#ifdef __cplusplus
extern "C" {
#endif

#define FFT_MAX_N 256

/* Work buffers of the DSP code are only touched by the CPU. With DSP_IN_CCM
 * (AudioGalaxy) they go to the F4's CCM RAM, which is otherwise unused there. */
#if defined(DSP_IN_CCM)
#define DSP_SCRATCH __attribute__((section(".ccmram")))
#else
#define DSP_SCRATCH
#endif

/* re/im of n points (n a power of two, <= FFT_MAX_N). Returns -1 if n is invalid. */
int  Fft_Run(float *re, float *im, unsigned n);
/* |X[k]| for k = 0..n/2-1 into mag (may alias re) */
void Fft_Magnitude(const float *re, const float *im, float *mag, unsigned n);

#ifdef __cplusplus
}
#endif

#endif
