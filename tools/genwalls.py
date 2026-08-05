# Genera g_TilePattern[29*8] / g_TileColor[29*8] para msx_coco.c segun
# DESIGN_v020.md secciones 1.2-1.4:
#  - 0        T_PATH (vacio)
#  - 1..16    T_WALL+mask: variante de pared por mascara de aristas.
#             bit0=N bit1=E bit2=S bit3=W; bit activo = el vecino-celda en esa
#             direccion es CAMINO => se dibuja contorno de 2 px en ese lado.
#             Primitivos: N=filas 0-1 0xFF, S=filas 6-7 0xFF, E=|=0x03, W=|=0xC0
#  - 17..20   esquinas interiores (tick 2x2 hacia la diagonal-camino)
#  - 21..24   cuartos del dot (dorado, indice 6)
#  - 25..28   cuartos del power pellet (circulo 12 px, indice 5 ciclado)
# La salida se pega en msx_coco.c (bloque marcado "generado por tools/genwalls.py").

NUM = 29
pat = [[0] * 8 for _ in range(NUM)]
col = [[0x11] * 8 for _ in range(NUM)]
names = [""] * NUM

def fill(idx, pattern, color, name):
    pat[idx] = pattern
    col[idx] = [color] * 8
    names[idx] = name

names[0] = "0: T_PATH"

for m in range(16):
    rows = [0] * 8
    for r in range(8):
        b = 0
        if (m & 1) and r < 2:
            b |= 0xFF                      # PRIM_N
        if (m & 4) and r >= 6:
            b |= 0xFF                      # PRIM_S
        if m & 2:
            b |= 0x03                      # PRIM_E
        if m & 8:
            b |= 0xC0                      # PRIM_W
        rows[r] = b
    sides = "".join(s for bit, s in ((1, "N"), (2, "E"), (4, "S"), (8, "W")) if m & bit)
    fill(1 + m, rows, 0x32, "%d: T_WALL+%d (%s)" % (1 + m, m, sides or "interior"))

fill(17, [0xC0, 0xC0, 0, 0, 0, 0, 0, 0], 0x82, "17: T_CNW")
fill(18, [0x03, 0x03, 0, 0, 0, 0, 0, 0], 0x82, "18: T_CNE")
fill(19, [0, 0, 0, 0, 0, 0, 0xC0, 0xC0], 0x82, "19: T_CSW")
fill(20, [0, 0, 0, 0, 0, 0, 0x03, 0x03], 0x82, "20: T_CSE")

fill(21, [0, 0, 0, 0, 0, 0x06, 0x07, 0x06], 0x61, "21: T_DOT_TL")
fill(22, [0, 0, 0, 0, 0, 0x60, 0xE0, 0x60], 0x61, "22: T_DOT_TR")
fill(23, [0x06, 0x07, 0x06, 0, 0, 0, 0, 0], 0x61, "23: T_DOT_BL")
fill(24, [0x60, 0xE0, 0x60, 0, 0, 0, 0, 0], 0x61, "24: T_DOT_BR")

fill(25, [0x00, 0x00, 0x03, 0x0F, 0x1F, 0x3F, 0x3F, 0x3F], 0x51, "25: T_PEL_TL")
fill(26, [0x00, 0x00, 0xC0, 0xF0, 0xF8, 0xFC, 0xFC, 0xFC], 0x51, "26: T_PEL_TR")
fill(27, [0x3F, 0x3F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00], 0x51, "27: T_PEL_BL")
fill(28, [0xFC, 0xFC, 0xFC, 0xF8, 0xF0, 0xC0, 0x00, 0x00], 0x51, "28: T_PEL_BR")

def emit(name, data):
    print("const u8 %s[%d * 8] =" % (name, NUM))
    print("{")
    for i in range(NUM):
        print("\t%s, // %s" % (", ".join("0x%02X" % b for b in data[i]), names[i]))
    print("};")

emit("g_TilePattern", pat)
print("")
emit("g_TileColor", col)
