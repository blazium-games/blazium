#!/usr/bin/env bash

# Generate .zip set of icons for Steam

# Make icons with transparent backgrounds and all sizes
for s in 16 24 32 48 64 128 256 512 1024; do
  convert -resize ${s}x$s -antialias \
          -background transparent \
          ../../icon.svg icon$s.png
done

# 16px tga file for library
convert icon16.png icon16.tga

# zip for Linux (7z ZIP for better DEFLATE; OS-native .zip format)
SEVENZ=""
if command -v 7z >/dev/null 2>&1; then
  SEVENZ="7z"
elif command -v 7za >/dev/null 2>&1; then
  SEVENZ="7za"
else
  echo "Error: 7z/7za required to create blazium-icons.zip" >&2
  exit 1
fi
rm -f blazium-icons.zip
"$SEVENZ" a -tzip -bso0 -bd -mx=9 blazium-icons.zip icon*.png

rm -f icon*.png
