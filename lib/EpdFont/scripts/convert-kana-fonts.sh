#!/bin/bash
# Kana practice fonts: Basic Latin + hiragana + katakana and the few symbols the
# kana quiz draws. Source: Noto Sans JP Medium (SIL OFL 1.1), from
# https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/JP/NotoSansJP-Medium.otf
# saved as ../builtinFonts/source/NotoSansJP/NotoSansJP-Medium.otf

set -e

cd "$(dirname "$0")"

KANA_INTERVALS=(
  --only-additional-intervals
  --additional-intervals 0x00D7,0x00D7  # multiplication sign (wrong mark)
  --additional-intervals 0x2192,0x2192  # rightwards arrow (next key)
  --additional-intervals 0x25CB,0x25CB  # white circle (right mark)
  --additional-intervals 0x3041,0x3096  # hiragana
  --additional-intervals 0x309B,0x309C  # spacing (han)dakuten marks
  --additional-intervals 0x30A1,0x30FC  # katakana and the long vowel mark
  --additional-intervals 0x5C0F,0x5C0F  # "small" (key label)
)

font_path="../builtinFonts/source/NotoSansJP/NotoSansJP-Medium.otf"
for size in 20 44; do
  python fontconvert.py kana_${size} ${size} "$font_path" --2bit --compress "${KANA_INTERVALS[@]}" \
    > ../builtinFonts/kana_${size}.h
  echo "Generated ../builtinFonts/kana_${size}.h"
done
