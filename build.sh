#!/bin/bash
#=============================================================================
# build.sh - compila msx_coco.com (MSX-DOS 2, via UNAPI) usando MSXgl
#
# Uso:
#   MSXGL=/ruta/a/MSXgl bash build.sh
#   (por defecto MSXGL=$HOME/MSXgl)
#
# Como funciona:
#   1. Copia los fuentes a <MSXgl>/projects/msx_coco/
#   2. Ejecuta build.js (compila + enlaza + empaqueta el .com DOS2)
#   3. Devuelve msx_coco.com a este directorio
#
# NOTA: Target=DOS2 en project_config.js. La config de cartucho (ROM_KONAMI) se
# conserva en project_config.rom.js por si hace falta regenerar el single-player.
#=============================================================================
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
MSXGL="${MSXGL:-$HOME/MSXgl}"

if [[ ! -d "$MSXGL/engine/script/js" ]]; then
    echo "[ERR] No encuentro MSXgl en: $MSXGL"
    echo "      Pasa la ruta con MSXGL=/ruta/a/MSXgl bash build.sh"
    exit 1
fi

DST="$MSXGL/projects/msx_coco"
mkdir -p "$DST"

echo "[+] Copiando fuentes a $DST"
# Phase B (multiplayer): incluimos *.h y *.asm (UNAPI) ademas de los *.c y configs.
# Patron copiado de msxai2/src/build.sh -> msxbuild.sh (shopt nullglob + lista).
shopt -s nullglob
for f in "$HERE"/*.c "$HERE"/*.h "$HERE"/*.asm "$HERE/msxgl_config.h" "$HERE/project_config.js"; do
    cp "$f" "$DST/"
done
shopt -u nullglob

echo "[+] Compilando msx_coco.com (Target=DOS2)"
cd "$DST"
if type -P node >/dev/null; then
    node ../../engine/script/js/build.js
else
    ../../tools/build/Node/node ../../engine/script/js/build.js
fi

# Localizar el .com generado. MSXgl deja el binario DOS2 en out/<ProjName>.com
# (a veces en MAYUSCULAS .COM). Busqueda flexible (patron de msxbuild.sh):
COM=""
for c in "$DST/out/msx_coco.com" "$DST/msx_coco.com" \
         "$DST/out/MSX_COCO.COM" "$DST/MSX_COCO.COM"; do
    if [[ -f "$c" ]]; then COM="$c"; break; fi
done
# Fallback: cualquier .com/.COM dentro de out/ (o del directorio del proyecto)
if [[ -z "$COM" ]]; then
    COM="$(find "$DST" -maxdepth 2 \( -name '*.com' -o -name '*.COM' \) -print -quit 2>/dev/null || true)"
fi
if [[ -z "$COM" ]]; then
    echo "[WARN] No encontre msx_coco.com. Revisa $DST manualmente:"
    find "$DST" -name '*.com' -o -name '*.COM' -o -name '*.ihx' 2>/dev/null | head
    exit 1
fi

cp "$COM" "$HERE/msx_coco.com"
echo "[OK] Compilado: $HERE/msx_coco.com ($(wc -c < "$HERE/msx_coco.com") bytes)"
