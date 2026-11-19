#!/bin/sh
# Build and run the host unit tests (hardware-independent firmware logic).
# Uses $CC if set, otherwise gcc. Works in a single-project repository
# (App/ next to tests/) and in the combined workspace (AudioGalaxy/, BeaconGalaxy/,
# GalaxyBoot/ next to tests/): the tests of every project found are built in.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT="$HERE/../.."
C="$ROOT/common"
CC=${CC:-gcc}
OUT="$HERE/build"
mkdir -p "$OUT"

# App/ folder of a project, if it is here
app_of() {
  if [ -d "$ROOT/$1/App" ]; then echo "$ROOT/$1/App"
  elif [ -f "$ROOT/$1.ioc" ]; then echo "$ROOT/App"
  fi
}
A=$(app_of AudioGalaxy)
B=$(app_of BeaconGalaxy)
G=$(app_of GalaxyBoot)

FLAGS="-I$C/cli -I$C/util -I$C/link"
SRCS="$C/cli/cli_parse.c $C/util/crc16.c $C/util/crc32.c $C/link/galaxy_link.c"
if [ -n "$A" ]; then
  FLAGS="$FLAGS -DTEST_AUDIO -DPLAYLIST_HOST_TEST -I$A/Player"
  SRCS="$SRCS $A/Player/wav.c $A/Player/playlist.c"
fi
if [ -n "$B" ]; then
  FLAGS="$FLAGS -DTEST_BEACON -I$B/Gif -I$B/Matrix -I$B/Eq"
  SRCS="$SRCS $B/Gif/gif_names.c $B/Gif/bmp.c $B/Matrix/matrix_px.c $B/Eq/fft.c"
fi
if [ -n "$G" ]; then
  FLAGS="$FLAGS -DTEST_BOOT -DBOOT_HOST_TEST -I$G/Boot"
  SRCS="$SRCS $G/Boot/image.c $G/Boot/boot_jump.c"
fi

# shellcheck disable=SC2086
"$CC" -std=c11 -Wall -Wextra -Werror -O1 $FLAGS "$HERE/test_main.c" $SRCS -lm -o "$OUT/host_tests"
"$OUT/host_tests"
