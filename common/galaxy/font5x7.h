/*
 * font5x7.h - the classic 5x7 pixel font, ' ' .. '_' (64 characters; lower
 * case is drawn as upper case). Shared by the GalaxyBoot screen and the small
 * BeaconGalaxy readouts: crisp at this size, with a slashed zero.
 *
 * Each character is 5 column bytes, left to right; bit 0 is the top row.
 */
#ifndef COMMON_FONT5X7_H
#define COMMON_FONT5X7_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT5X7_W       5
#define FONT5X7_H       7
#define FONT5X7_ADVANCE 6       /* one blank column between characters */
#define FONT5X7_COUNT   64

extern const uint8_t Font5x7[FONT5X7_COUNT][5];

/* the columns of c ('?' for characters the font does not have) */
const uint8_t *Font5x7_Glyph(char c);

#ifdef __cplusplus
}
#endif

#endif
