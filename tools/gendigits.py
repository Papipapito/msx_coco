# Genera patrones 16x16 de digitos 0-9 en orden de cuadrantes MSX (TL,BL,TR,BR)
FONT = {
    0: ["111","101","101","101","111"],
    1: ["010","110","010","010","111"],
    2: ["111","001","111","100","111"],
    3: ["111","001","111","001","111"],
    4: ["101","101","111","001","001"],
    5: ["111","100","111","001","111"],
    6: ["111","100","111","101","111"],
    7: ["111","001","001","001","001"],
    8: ["111","101","111","101","111"],
    9: ["111","101","111","001","111"],
}

def make_bitmap(digit):
    rows = FONT[digit]
    # bloque de 4 px ancho x 3 px alto; 3 cols -> 12 px, 5 filas -> 15 px
    # centrar en 16x16: x offset 2, y offset 0
    img = [[0]*16 for _ in range(16)]
    for r in range(5):
        for c in range(3):
            if rows[r][c] == '1':
                for dy in range(3):
                    for dx in range(4):
                        img[r*3 + dy][2 + c*4 + dx] = 1
    return img

def to_msx(img):
    out = []
    for (r0, r1, c0, c1) in [(0,8,0,8),(8,16,0,8),(0,8,8,16),(8,16,8,16)]:
        for r in range(r0, r1):
            b = 0
            for c in range(c0, c1):
                if img[r][c]:
                    b |= (0x80 >> (c - c0))
            out.append(b)
    return out

print("const u8 g_DigitPattern[10 * 4 * 8] =")
print("{")
for d in range(10):
    b = to_msx(make_bitmap(d))
    print("\t" + ", ".join("0x%02X" % x for x in b) + ", // %d" % d)
print("};")
