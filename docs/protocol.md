# Board-to-board protocol

BeaconGalaxy (F405 board) controls AudioGalaxy (F407 Discovery) over a serial
link. The code for both sides is `common/link/galaxy_link.c/.h`, and the host
tests cover it.

## Wiring and framing

- USART2 on both boards: PA2 (TX) and PA3 (RX), crossed, plus ground. On the
  Galaxy board this is connector J7.
- 115200 baud, 8N1, no flow control.
- ASCII lines ending in CR or LF (CR LF counts once). Empty lines are ignored.
  The longest line is 128 characters, and longer lines are dropped.
- Words are separated by spaces. A word containing spaces is written in
  "double quotes", with `\"` and `\\` as escapes.
- Verbs are not case-sensitive.

## BeaconGalaxy → AudioGalaxy

| Command | Meaning |
|---|---|
| `GALAXYHELLO` | Asks AudioGalaxy to answer with `GALAXYREADY`. |
| `GALAXYGETPLAYLIST` | Asks for the playlist (one `PLAYLIST` line per track). |
| `GALAXYPLAY <index>` | Plays track `index` (0-based). |
| `GALAXYPLAYTHEME <index>` | Plays track `index` as theme music. |
| `GALAXYVOL <0..100>` | Sets the volume now. |
| `GALAXYRECALLVOL <0..100>` | Sets the stored volume at start-up. AudioGalaxy confirms it. |
| `GALAXYSTOP` / `GALAXYSTART` | Stops / starts (or resumes) playing. |
| `GALAXYPAUSE` / `GALAXYRESUME` | Pauses / resumes. |
| `GALAXYREPEATON` / `GALAXYREPEATOFF` | Repeats the current track or not. AudioGalaxy confirms it. |
| `GALAXYRECORD` / `GALAXYSTOPRECORD` | Records `rec.wav` from the microphone (30 s at most) / stops recording. |

## AudioGalaxy → BeaconGalaxy

| Event | Meaning |
|---|---|
| `GALAXYREADY <tracks>` | Sent at start-up, when a USB stick is mounted, and in reply to `GALAXYHELLO`. |
| `PLAYLIST "<name>" <index> <total>` | One track. |
| `GALAXYAUDIO PLAYING <index>` | A track started. |
| `GALAXYAUDIO WAVEEND` | The current track ended (only when repeat is off). |
| `GALAXYAUDIO STOPPED` | Playing was stopped. |
| `GALAXYAUDIO RECALLVOL` | `GALAXYRECALLVOL` was applied. |
| `GALAXYAUDIO REPEATSET` | `GALAXYREPEATON/OFF` was applied. |
| `GALAXYAUDIO NOUSB` | There is no USB stick, or no tracks on it. |
| `GALAXYAUDIO ERR "<reason>"` | A command could not be done. |

## Handshake

The boards can be powered in any order. BeaconGalaxy's App task (`app_main.c`)
keeps the link in step:

1. Until a `GALAXYREADY` arrives, BeaconGalaxy sends `GALAXYHELLO` every 2 s.
2. On every `GALAXYREADY` with tracks, it sends `GALAXYGETPLAYLIST`. It also
   sends the stored volume and repeat mode (`GALAXYRECALLVOL`,
   `GALAXYREPEATON/OFF`).
3. A playlist that stops arriving before it is complete is requested again,
   after 1.5 s without a new line and up to 5 times.
4. Volume and repeat are sent again until AudioGalaxy confirms both, up to 5
   times. A new `GALAXYREADY` means AudioGalaxy restarted, so both must be sent
   again.

Autoplay of tracks works by turning repeat off and starting the next track on
each `GALAXYAUDIO WAVEEND`.

## Trying it by hand

- The AudioGalaxy console accepts the same commands through
  `gx <GALAXY...>`, for example `gx GALAXYPLAY 3`.
- The BeaconGalaxy console sends a raw line with `gx <line>`.
- On BeaconGalaxy, `link log on` prints every line in both directions.
