#!/bin/sh
# Assemble a Z80 source file into Intel HEX for Z80Explorer.
#
# Pass an .asm file as the first argument. The .hex lands in the same directory as the source, just like
# make_test.bat does on Windows. Auto-detects host OS and picks the matching zmac binary.

set -e

here="$(cd -- "$(dirname -- "$0")" && pwd)"

case "$(uname -s)" in
    Darwin)  zmac="$here/zmac.macos" ;;
    Linux)   zmac="$here/zmac.linux" ;;
    *)       echo "Unsupported OS: $(uname -s)" >&2; exit 1 ;;
esac

if [ ! -x "$zmac" ]; then
    echo "zmac binary not found or not executable: $zmac" >&2
    echo "Build it from http://48k.ca/zmac.html and place the binary alongside this script." >&2
    exit 1
fi

src="$1"
if [ -z "$src" ]; then
    echo "Usage: $0 <file.asm>" >&2
    exit 1
fi

base="$(basename -- "$src" .asm)"
srcdir="$(cd -- "$(dirname -- "$src")" && pwd)"

cd "$srcdir"
"$zmac" --zmac --oo hex,bds,lst "$src"
cp "zout/${base}.hex" "./${base}.hex"
