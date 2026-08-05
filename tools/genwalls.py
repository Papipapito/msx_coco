# Genera g_TilePattern[29*8] / g_TileColor[29*8] para msx_coco.c segun
# DESIGN_v020 1.2-1.4 + requisitos del usuario (estetica neon):
#  - 0        T_PATH (vacio)
#  - 1..16    T_WALL+mask: variante de pared por mascara de aristas.
#             bit0=N bit1=E bit2=S bit3=W; bit activo = el vecino-celda en esa
#             direccion es CAMINO => contorno neon de 2 px en ese lado.
#             Primitivos: N=filas 0-1 0xFF, S=filas 6-7 0xFF, E=|=0x03, W=|=0xC0
#             Relleno con TEXTURA SCANLINE: filas pares bg=2, impares bg=12
#             (solo el byte de color por linea, cero patrones extra).
#  - 17..20   esquinas interiores (tick 2x2 hacia la diagonal-camino), fg=8
#  - 21..24   cuartos de la GEMA (rombo 6x6, 2 px punta / 6 px centro):
#             mitad superior fg=6 oro brillante, mitad inferior fg=10 ambar
#  - 25..28   cuartos del power pellet (circulo 12 px, indice 5 ciclado)
# La salida se pega en msx_coco.c (bloque marcado "generado por tools/genwalls.py").

NUM = 29
pat = [[0] * 8 for _ in range(NUM)]
col = [[0x11] * 8 for _ in range(NUM)]
names = [""] * NUM

# Byte de color por linea de pared: fg 3 (contorno neon), bg 2/12 alternando
WALL_COLS   = [0x32 if r % 2 == 0 else 0x3C for r in range(8)]
# Esquinas: fg 8 (acento), mismo relleno scanline
CORNER_COLS = [0x82 if r % 2 == 0 else 0x8C for r in range(8)]

def fill(idx, pattern, colors, name):
    pat[idx] = pattern
    col[idx] = colors if isinstance(colors, list) else [colors] * 8
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
    fill(1 + m, rows, list(WALL_COLS), "%d: T_WALL+%d (%s)" % (1 + m, m, sides or "interior"))

fill(17, [0xC0, 0xC0, 0, 0, 0, 0, 0, 0], list(CORNER_COLS), "17: T_CNW")
fill(18, [0x03, 0x03, 0, 0, 0, 0, 0, 0], list(CORNER_COLS), "18: T_CNE")
fill(19, [0, 0, 0, 0, 0, 0, 0xC0, 0xC0], list(CORNER_COLS), "19: T_CSW")
fill(20, [0, 0, 0, 0, 0, 0, 0x03, 0x03], list(CORNER_COLS), "20: T_CSE")

# Gema 6x6: rombo 2/4/6 - 6/4/2. Cada cuarto: 1 px en la punta, 3 px en el
# centro. Cuartos superiores en oro brillante (0x61), inferiores en ambar (0xA1).
fill(21, [0, 0, 0, 0, 0, 0x01, 0x03, 0x07], [0x11] * 5 + [0x61] * 3, "21: T_DOT_TL (gema sup-izq)")
fill(22, [0, 0, 0, 0, 0, 0x80, 0xC0, 0xE0], [0x11] * 5 + [0x61] * 3, "22: T_DOT_TR (gema sup-der)")
fill(23, [0x07, 0x03, 0x01, 0, 0, 0, 0, 0], [0xA1] * 3 + [0x11] * 5, "23: T_DOT_BL (gema inf-izq)")
fill(24, [0xE0, 0xC0, 0x80, 0, 0, 0, 0, 0], [0xA1] * 3 + [0x11] * 5, "24: T_DOT_BR (gema inf-der)")

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
