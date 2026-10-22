#!/usr/bin/env bash
# Builds and runs both programs.
#   ./run.sh         real rates from the internet
#   ./run.sh demo    real rates + two labelled fake mispricings (shows a profitable route)
set -e
cd "$(dirname "$0")"

CXX=$(command -v clang++ || command -v g++ || true)
CC=$(command -v clang || command -v gcc || true)
if [ -z "$CXX" ] || [ -z "$CC" ]; then
  echo "No compiler found. On macOS run: xcode-select --install"
  exit 1
fi

echo "=== Compiling ==="
"$CXX" -std=c++17 market_builder.cpp -o market_builder
"$CC" arbitrage.c -o arbitrage -lm
echo "Done."
echo

if [ "$1" = "demo" ]; then ./market_builder --demo; else ./market_builder; fi
echo
./arbitrage
