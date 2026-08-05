#!/bin/bash
#=============================================================================
# build.sh - compila msx_coco.rom usando MSXgl
#
# Uso:
#   MSXGL=/ruta/a/MSXgl bash build.sh
#   (por defecto MSXGL=$HOME/MSXgl)
#
# Como funciona:
#   1. Copia los fuentes a <MSXgl>/projects/msx_coco/
#   2. Ejecuta build.js (compila + enlaza + empaqueta el ROM)
#   3. Devuelve msx_coco.rom a este directorio
#=============================================================================
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
MSXGL="${MSXGL:-$HOME/MSXgl}"

if [[ ! -d "$MSXGL/engine/script/js" ]]; then
    echo "[ERR] No encuentro MSXgl en: $MSXGL"
    echo "      Pasa la ruta con MSXGL=/ruta/a/MSXgl bash build.sh"
    exit 1
fi

# Staging propio (msx_coco_v2) para no pisar el staging del worktree principal
DST="$MSXGL/projects/msx_coco_v2"
mkdir -p "$DST"

echo "[+] Copiando fuentes a $DST"
cp "$HERE"/*.c              "$DST/"
cp "$HERE/msxgl_config.h"   "$DST/"
cp "$HERE/project_config.js" "$DST/"

echo "[+] Compilando msx_coco.rom"
cd "$DST"
if type -P node >/dev/null; then
    node ../../engine/script/js/build.js
else
    ../../tools/build/Node/node ../../engine/script/js/build.js
fi

# Localizar el .rom generado
ROM="$(find "$DST" -maxdepth 2 -name 'msx_coco.rom' -print -quit 2>/dev/null || true)"
if [[ -z "$ROM" ]]; then
    echo "[WARN] No encontre msx_coco.rom. Revisa $DST manualmente:"
    find "$DST" -name '*.rom' -o -name '*.ihx' 2>/dev/null | head
    exit 1
fi

cp "$ROM" "$HERE/msx_coco.rom"
echo "[OK] Compilado: $HERE/msx_coco.rom ($(wc -c < "$HERE/msx_coco.rom") bytes)"
