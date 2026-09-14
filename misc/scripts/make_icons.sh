#!/usr/bin/env bash

# Generate .ico, .icns and .zip set of icons for Steam

# Make icons with transparent backgrounds and all sizes
for s in 16 24 32 48 64 128 256 512 1024; do
  convert -resize ${s}x$s -antialias \
          -background transparent \
          ../../misc/dist/icon_generation/icon.svg icon$s.png
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

# ico for Windows
# Not including biggest ones or it blows up in size
icotool -c -o blazium-icon.ico icon{16,24,32,48,64,128,256}.png

# icns for macOS
# Only some sizes: https://iconhandbook.co.uk/reference/chart/osx/
png2icns blazium-icon.icns icon{16,32,128,256,512,1024}.png

rm -f icon*.png
