# Genera patrones de sprite 16x16 en orden MSX (cuadrantes TL,BL,TR,BR).
#  - Comecocos: circulo cerrado + boca abierta en 4 direcciones.
#  - Fantasma: 2 frames (faldon animado) con ojos (huecos).
closed = [
"......####......",
"....########....",
"...##########...",
"..############..",
".##############.",
".##############.",
"################",
"################",
"################",
"################",
".##############.",
".##############.",
"..############..",
"...##########...",
"....########....",
"......####......",
]
open_right = [
"......####......",
"....########....",
"...##########...",
"..#########.....",
".########.......",
".#######........",
"######..........",
"#####...........",
"#####...........",
"######..........",
".#######........",
".########.......",
"..#########.....",
"...##########...",
"....########....",
"......####......",
]
# Fantasma frame A (cupula con ojos + faldon)
ghost_a = [
"......####......",
"....########....",
"...##########...",
"..############..",
".##############.",
".###..####..###.",
".###..####..###.",
"################",
"################",
"################",
"################",
"################",
"################",
"###..###..###..#",
"##....##....##..",
"#......#......#.",
]
# Fantasma frame B (faldon desfasado)
ghost_b = [
"......####......",
"....########....",
"...##########...",
"..############..",
".##############.",
".###..####..###.",
".###..####..###.",
"################",
"################",
"################",
"################",
"################",
"################",
"#..###..###..###",
"..##....##....##",
".#......#......#",
]
def grid(img): return [[1 if c == '#' else 0 for c in row] for row in img]
def flip_h(g): return [row[::-1] for row in g]
def rot_cw(g):  return [[g[15 - c][r] for c in range(16)] for r in range(16)]
def rot_ccw(g): return [[g[c][15 - r] for c in range(16)] for r in range(16)]
def to_msx(g):
    # Orden de cuadrantes del hardware MSX (column-major): TL, BL, TR, BR.
    # Verificado a resolucion nativa con tools/analyze.py sobre capturas -raw.
    out = []
    for (r0, r1, c0, c1) in [(0, 8, 0, 8), (8, 16, 0, 8), (0, 8, 8, 16), (8, 16, 8, 16)]:
        for r in range(r0, r1):
            b = 0
            for c in range(c0, c1):
                if g[r][c]:
                    b |= (0x80 >> (c - c0))
            out.append(b)
    return out
def emit(name, frames):
    n = len(frames)
    print("const u8 %s[%d * 4 * 8] =\n{" % (name, n))
    for fname, g in frames:
        b = to_msx(g)
        print("\t%s, // %s" % (", ".join("0x%02X" % x for x in b), fname))
    print("};")
gr = grid(open_right)
pac = [("CLOSED", grid(closed)), ("OPEN_R", gr), ("OPEN_L", flip_h(gr)),
       ("OPEN_U", rot_ccw(gr)), ("OPEN_D", rot_cw(gr))]
ghost = [("GHOST_A", grid(ghost_a)), ("GHOST_B", grid(ghost_b))]
emit("g_PacPattern", pac)
print("")
emit("g_GhostPattern", ghost)
