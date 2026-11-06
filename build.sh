#!/bin/bash
# build.sh [engine...]  - builds <engine> (linux) and <engine>.exe (windows) for each engine .c
set -e
cd "$(dirname "$0")"
engines="$@"
[ -z "$engines" ] && engines=$(ls *.c | sed 's/\.c$//')
for e in $engines; do
  gcc -O2 -Wall -Wextra -o "$e" "$e.c" -lpthread -lm
  x86_64-w64-mingw32-gcc -O2 -static -o "$e.exe" "$e.c" -lpthread -lm
  echo "built $e $e.exe"
done
