#!/bin/sh
# cubemx_generate.sh <project-dir> - regenerate a project from its .ioc with
# STM32CubeMX 6.15, headless. USER CODE blocks and App/ are kept.
set -e
DIR=$(cd "$1" && pwd -W)
NAME=$(basename "$DIR")
MX="${CUBEMX:-$LOCALAPPDATA/Programs/STM32CubeMX/STM32CubeMX.exe}"
SCRIPT="$DIR/build/cubemx_script.txt"
mkdir -p "$DIR/build"
cat > "$SCRIPT" <<EOF
config load $DIR/$NAME.ioc
project generate
exit
EOF
"$MX" -q "$SCRIPT" > "$DIR/build/cubemx.log" 2>&1 || true
# The launcher can return while CubeMX (javaw) is still generating: wait for its
# "Bye bye" (up to 5 minutes) before reporting.
for i in $(seq 1 300); do
  grep -q "Bye bye" "$DIR/build/cubemx.log" 2>/dev/null && break
  sleep 1
done
grep -E "ERROR|SEVERE|Generation|OK|KO" "$DIR/build/cubemx.log" | grep -vE "^\s*$" | tail -25
