#!/bin/sh
# ci_build.sh - what CI checks, runnable on a PC too:
#   1. build the firmware with the Debug (-O0) and Release (-Os) presets: this
#      project, or all three in the combined workspace
#   2. fail on any compiler warning in our own code (App/, common/, TouchGFX/gui/,
#      TouchGFX/target/); warnings in CubeMX / ST / TouchGFX generated code are listed only
#   3. run the host unit tests
# Needs arm-none-eabi-gcc, cmake, ninja and a host gcc on the PATH (CC may name it).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
STATUS=0

# a single-project repository, or the combined workspace with all three
if [ -f "$ROOT/CMakePresets.json" ]; then PROJECTS=.; else PROJECTS="AudioGalaxy BeaconGalaxy GalaxyBoot"; fi

for P in $PROJECTS; do
  for PRESET in Debug Release; do
    echo "=== $P $PRESET"
    cd "$ROOT/$P"
    cmake --preset $PRESET > /dev/null
    LOG="$ROOT/$P/build/$PRESET/ci_build.log"
    mkdir -p "$ROOT/$P/build/$PRESET"
    # a clean rebuild, so every warning shows up again
    cmake --build --preset $PRESET --clean-first > "$LOG" 2>&1 || { cat "$LOG"; echo "FAILED: $P ($PRESET) does not build"; exit 1; }
    grep -E "FLASH:|RAM:" "$LOG" || true
    OWN=$(grep -E "warning:" "$LOG" | grep -E "/(App|common|TouchGFX/gui|TouchGFX/target)/" || true)
    OTHER=$(grep -cE "warning:" "$LOG" || true)
    if [ -n "$OWN" ]; then
      echo "$OWN"
      echo "FAILED: warnings in $P's own code ($PRESET)"
      STATUS=1
    fi
    echo "warnings: own code $( [ -n "$OWN" ] && echo "$OWN" | wc -l || echo 0 ), total $OTHER"
  done
done

echo "=== host tests"
sh "$ROOT/tests/host/run_tests.sh" || STATUS=1
exit $STATUS
