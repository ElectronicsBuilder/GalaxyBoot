# Beacon Galaxy: how the firmware is built

The system has three firmwares:

- **BeaconGalaxy:** the matrix, menu and controller application.
- **GalaxyBoot:** its bootloader.
- **AudioGalaxy:** the audio player, on a separate board.

All three are STM32CubeMX projects (STM32CubeMX 6.15, STM32Cube FW_F4 1.28.3)
built with CMake and Ninja. CubeMX owns `Core/`, `Drivers/`, `Middlewares/`
and the CubeMX parts of `FATFS/` and `USB_HOST/`. Our code lives in `App/`
and `common/`, plus the USER CODE blocks.

## Hardware

| | Galaxy board | AudioGalaxy |
|---|---|---|
| MCU | STM32F405RG, 8 MHz crystal, 168 MHz | STM32F407VG (STM32F407G-DISC1), 168 MHz |
| Display | 22×22 WS2812B matrix on PB0; 64×48 SSD1306 OLED on SPI1 | — |
| Input | 4 buttons PC0/PC1/PC2/PC7 (UP, DOWN, BACK, ENTER), active high | user button PA0 |
| Storage | microSD on SDIO (4-bit), M24C512 EEPROM on I2C2 | USB stick (USB host, FS) |
| Audio | microphone on PA0 (ADC1 IN0), for the equalizer | CS43L22 codec (I2S3 + I2C1), MP45DT02 PDM microphone (I2S2) |
| Status LEDs | PB6 green, PB7 orange, PB8 red, PB9 blue (TIM4 PWM) | PD12–PD15 |
| Console | USART1 through the on-board FT232, 115200 | USART3, PD8 TX / PD9 RX, 115200 |
| Link | USART2, PA2/PA3 (J7) | USART2, PA2/PA3 |

Both boards also have a console over SWD: `python tools/swd_console.py <elf>
<st-link serial> "help"`. It needs no wiring and leaves the firmware running.

## BeaconGalaxy

FreeRTOS (CMSIS-RTOS2) runs these tasks:

- **App** (`App/Main/app_main.c`), every 2 ms:
  - console input;
  - the link to AudioGalaxy and its handshake (see [protocol.md](protocol.md));
  - buttons;
  - the commands the GUI posts;
  - the SD-card supervisor;
  - the status LEDs.
- **gui:** TouchGFX 4.27 on the 1-bit OLED.
  - The custom HAL is in `TouchGFX/target/`, with VSync from a 16 ms timer.
  - The GUI reaches the rest of the firmware only through `App/Ui/ui_bridge.h`:
    it posts typed commands and reads status through getters.
  - Screen logic is in `TouchGFX/gui/`; shared navigation state is in
    `gui/common/Ui.hpp`.
  - Widgets drawn in code (`gui/common/`): `AudioMeter` (spectrum / VU from
    `GALAXYAUDIO LEVELS`, automatic range), `TrackProgress`, `MenuBar` (menu
    position), `PixelText` (the 5x7 font of `common/galaxy/font5x7.c`, shared
    with GalaxyBoot, for small readouts; Verdana at 7 px blurs its zero into
    a D). `ui::TextScroll` scrolls text that is longer than its place.
  - `tools/oled_shot.py` reads the framebuffer over SWD and saves a PNG of
    the screen, optionally after pressing keys: the screens can be checked
    without looking at the board.
- **fx** (`App/Fx/`): owns the LED matrix and runs one effect at a time: a
  GIF, a theme, the equalizer, a plain colour or a test pattern.
  - `App/Matrix/ws2812.c`: TIM3 CH3 PWM, refilled by DMA1 stream 2 from a
    small two-half buffer that the interrupt encodes, 4 LEDs at a time.
  - `App/Matrix/matrix.c`: framebuffer, serpentine chain order, brightness
    and gamma.
  - `App/Gif/`: frames are BMP files read row by row, 24/32-bit, scaled to
    22×22. When a GIF starts, one directory pass notes where each frame
    starts on the card. Opening a file by name searches a root folder of
    thousands of files (about 95 ms); with the index a frame loads in about
    7 ms.
  - Themes: Fire, Meteor, Bouncing, Plasma, Stars, Rain, Swirl, Life.
  - `App/Eq/`: the microphone is sampled at 16 kHz (TIM2 → ADC1 → DMA2). A
    256-point FFT drives 22 log-spaced bands, shown as bars or a VU meter,
    with automatic gain and a noise gate (`fx gate <dB>`).

### SD card without a card-detect switch

The board has no detect pin. CubeMX's FatFs code needs one, so it is given
PC13, which is not connected, with a pull-down: it always reads "present".

- **Finding the card:** the card is found by mounting it.
- **Noticing removal:** every 2 s the card is asked for its status, and read
  errors also count.
- **When the card is gone:**
  - a GIF using it is stopped;
  - the volume is unmounted only after the GIF has stopped;
  - mounting is retried every 2 s.
- **Message:** after 10 s without a card, the start screen shows NO SD.
- **Read timeout:** in `FATFS/Target/sd_diskio.c` (USER CODE), the read
  timeout is 2 s instead of 30 s, and an SDIO error ends a waiting read at
  once.

### Watchdog

`common/stm32f4/watchdog.c` drives the independent watchdog (IWDG). It runs
from the internal 32 kHz LSI clock, so no stuck clock or peripheral can stop
it.

- **How supervision works:** the watchdog is refreshed only while every
  supervised task has reported within 3 s:
  - BeaconGalaxy: the App task, the GUI (every frame) and fx;
  - AudioGalaxy: the App task and the player.
- **Timeout:** 4 s once running, and 8 s during start-up, which includes the
  SD mount and the GIF scan. So a single stuck task resets the board within
  about 7 s.
- **Reset report:** before the reset, the stuck task's name is noted in the
  last bytes of the CCM RAM, which survive the reset. The next boot logs it,
  and `ver` / `wdg` report it, for example
  `last_reset=watchdog (task fx stuck)`.
- **Debugging:** the watchdog pauses while a debugger halts the core.
- **Testing:** `wdg stall <task>` stops counting one task's reports, and
  `wdg hang` stops the App task.

### Settings

`App/Settings` holds colour, intensity, volume, the editor step, GIF frame
rate, autoplay timing, LED effect, delay and DMA indicator, repeat, and the
autoplay orders. They are stored in the EEPROM as one record with a CRC (see
[memory-map.md](memory-map.md)).

## GalaxyBoot

Bare metal, no RTOS. GalaxyBoot updates the application when the EEPROM boot
fuse is set or ENTER is held at reset; otherwise it jumps to the application
straight away. The update steps and their safety are described in
[memory-map.md](memory-map.md).

- It reads the card without DMA, so the SDIO clock is 8 MHz (`ClockDiv 4`).
  At 24 MHz the FIFO overruns.
- Without a valid application it shows NO APP, and ENTER tries the SD card.
- Progress is shown on the OLED and on USART1. The last attempt is kept in
  RAM (`boot_diag`) for reading over SWD.
- The watchdog (8 s) runs during an update and in the NO APP wait, and is
  refreshed at every step. A hang resets the board, and the boot fuse, which
  is still set, makes it try again. A normal start jumps to the application
  with the watchdog off; the application starts its own.

## AudioGalaxy

- **Player** (`App/Player`): one task fed by a command queue.
  - `wav.c` parses RIFF properly. Mono and 8-bit files are converted to
    16-bit stereo.
  - Playback uses a ping-pong DMA buffer.
  - Pause keeps the codec powered and plays silence. Stop powers the codec
    down before its clock stops; done the other way round, the CS43L22 left
    the speaker hissing.
- **Recording:** `rec.wav`, 30 s at most, from the PDM microphone (ST's
  PDM2PCM library).
- **Link** (`App/Link`): parses the commands in [protocol.md](protocol.md) and
  reports events back.

## Building, testing, flashing

```
cmake --preset Release        # or Debug
cmake --build --preset Release
```

- **Which preset:** `Release` (`-Os`, no debug information) is the build to
  run on the boards. `Debug` (`-O0 -g3`) is for stepping through code in a
  debugger.
- **Both are tested:** both build with no warnings, and the Release builds
  were tested on the hardware: start-up through GalaxyBoot, the link,
  GIFs/themes/equalizer, audio, the SD update and the watchdog.

- **Toolchain:** STM32CubeCLT 1.16 (arm-none-eabi-gcc 12.3, CMake, Ninja),
  on the PATH.
- **Host tests:** `sh tests/host/run_tests.sh` with any PC gcc. They cover the
  protocol, CRC, ring buffer, command-line parsing, WAV, playlist, GIF names,
  BMP, matrix layout, FFT and the update-image checks.
- **CI checks:** `sh tools/ci_build.sh` does a clean build of Debug and Release
  (no warnings allowed in our own code) and runs the host tests. GitHub
  Actions runs the same script and keeps the Release images.
- **Flashing:** with STM32_Programmer_CLI, choosing the probe by serial number
  (VS Code tasks are included):
  `STM32_Programmer_CLI -c port=SWD sn=<serial> mode=UR -w build/Release/<name>.elf -v -rst`.
- **Regenerating CubeMX code:** `sh tools/cubemx_generate.sh <project-dir>`.
  Changes to the `.ioc` are kept as text in `cubemx/<name>.patch`.

## SD card

- **GIFs:** frames named `<NAME>-<n>.bmp` in the root folder. The name can be
  up to 16 characters, and long file names are fine. Frames are 24- or
  32-bit BMP, ideally 22×22; other sizes up to 128 pixels wide are scaled.
- **Updates:** `BeaconGalaxy.bin` in the root folder, then About › UPGRADE, or
  hold ENTER at reset.
