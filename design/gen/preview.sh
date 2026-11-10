#!/bin/bash
# Rend chaque SVG en PNG via Firefox headless (polices injectées pour l'aperçu seulement).
# usage: preview.sh <dossier_svg> <dossier_png> [fichier...]
set -e
SRC=$1; OUT=$2; shift 2
FONTS=${FONTS:?dossier des polices}
mkdir -p "$OUT"; PROF=$(mktemp -d -p "$OUT")
STYLE="<style>@font-face{font-family:'Chakra Petch';src:url(ChakraPetch-Bold.ttf);font-weight:700}@font-face{font-family:'Chakra Petch';src:url(ChakraPetch-SemiBold.ttf);font-weight:600}@font-face{font-family:'Chakra Petch';src:url(ChakraPetch-Medium.ttf);font-weight:500}@font-face{font-family:'Chakra Petch';src:url(ChakraPetch-BoldItalic.ttf);font-weight:700;font-style:italic}@font-face{font-family:'Rajdhani';src:url(Rajdhani-Bold.ttf);font-weight:700}@font-face{font-family:'Rajdhani';src:url(Rajdhani-SemiBold.ttf);font-weight:600}@font-face{font-family:'Rajdhani';src:url(Rajdhani-Medium.ttf);font-weight:500}</style>"
FILES=${@:-$(cd "$SRC" && ls *.svg)}
for f in $FILES; do
  b=${f%.svg}; w=$(grep -o 'width="[0-9]*"' "$SRC/$f" | head -1 | grep -o '[0-9]*'); h=$(grep -o 'height="[0-9]*"' "$SRC/$f" | head -1 | grep -o '[0-9]*')
  sed "0,/<defs>/s||$STYLE<defs>|" "$SRC/$f" > "$FONTS/_$f"
  firefox --headless --no-remote --profile "$PROF" --window-size=$w,$h --screenshot "$OUT/$b.png" "file://$FONTS/_$f" >/dev/null 2>&1
  rm -f "$FONTS/_$f"
done
rm -rf "$PROF"
