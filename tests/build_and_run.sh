#!/usr/bin/env bash
# Build and run the MIDI mapper test harness.
# Adds the winget-installed MinGW-w64 to PATH for this shell only.
set -e

MINGW_BIN="/c/Users/west/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/mingw64/bin"
if [ -x "$MINGW_BIN/g++.exe" ]; then
  export PATH="$MINGW_BIN:$PATH"
fi

cd "$(dirname "$0")"

g++ -O2 -std=c++17 -Wall -Wextra test_midi_mapper.cpp -o test_midi_mapper.exe
echo
./test_midi_mapper.exe
