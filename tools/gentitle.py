# Genera las letras grandes del titulo y del game over como tiles 8x8.
# Solo texto (el marco usa los tiles de muro y el comecocos/fantasmas son sprites).
import math

W, H = 256, 192
BLACK = 1

FONT = {
    'M': ["10001","11011","10101","10101","10001","10001","10001"],
    'S': ["01111","10000","10000","01110","00001","00001","11110"],
    'X': ["10001","10001","01010","00100","01010","10001","10001"],
    'C': ["01110","10001","10000","10000","10000","10001","01110"],
    'O': ["01110","10001","10001","10001","10001","10001","01110"],
    'G': ["01110","10001","10000","10111","10001","10001","01111"],
    'A': ["01110","10001","10001","11111","10001","10001","10001"],
    'E': ["11111","10000","10000","11110","10000","10000","11111"],
    'V': ["10001","10001","10001","10001","10001","01010","00100"],
    'R': ["11110","10001","10001","11110","10100","10010","10001"],
    'P': ["11110","10001","10001","11110","10000","10000","10000"],
    'U': ["10001","10001","10001","10001","10001","10001","01110"],
    'L': ["10000","10000","10000","10000","10000","10000","11111"],
}

class Canvas:
    def __init__(self):
        self.px = [[BLACK]*W for _ in range(H)]
    def setp(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = c
    def text(self, x, y, s, c, scale):
        cx = x
        for ch in s:
            if ch == ' ':
                cx += 4*scale
                continue
            g = FONT[ch]
            for ry in range(7):
                for rx in range(5):
                    if g[ry][rx] == '1':
                        for dy in range(scale):
                            for dx in range(scale):
                                self.setp(cx + rx*scale + dx, y + ry*scale + dy, c)
            cx += 6*scale

def text_width(s, scale):
    return sum(6*scale if ch != ' ' else 4*scale for ch in s)

def convert(cv):
    tiles = {}
    patterns = []
    colors = []
    nt = []
    for ty in range(H//8):
        for tx in range(W//8):
            key = tuple(tuple(cv.px[ty*8+py][tx*8:tx*8+8]) for py in range(8))
            if key not in tiles:
                tiles[key] = len(patterns)
                pat, col = tile_bytes(key)
                patterns.append(pat)
                colors.append(col)
            nt.append(tiles[key])
    return patterns, colors, nt

def tile_bytes(key):
    pat = []
    col = []
    for row in key:
        nonblack = [c for c in row if c != BLACK]
        uniq = list(dict.fromkeys(nonblack))
        if len(uniq) == 0:
            pat.append(0); col.append((BLACK<<4)|BLACK)
        elif len(uniq) == 1:
            c = uniq[0]; p = 0
            for i, cc in enumerate(row):
                if cc != BLACK: p |= 0x80 >> i
            pat.append(p); col.append((c<<4)|BLACK)
        else:
            fg, bg = uniq[0], uniq[1]; p = 0
            for i, cc in enumerate(row):
                if cc == fg: p |= 0x80 >> i
            pat.append(p); col.append((fg<<4)|bg)
    return pat, col

def emit(name, arr, per_line=16):
    print(f"const u8 {name}[{len(arr)}] =")
    print("{")
    for i in range(0, len(arr), per_line):
        print("\t" + ", ".join("0x%02X" % a for a in arr[i:i+per_line]) + ",")
    print("};")
    print()

# ---------- TITULO: "MSX COCO" (amarillo) ----------
cv = Canvas()
s = 4
txt = "MSX COCO"
cv.text((W - text_width(txt, s))//2, 28, txt, 11, s)   # 11 = amarillo
p_t, c_t, nt_t = convert(cv)

# ---------- GAME OVER: "GAME OVER" (rojo) ----------
cv2 = Canvas()
cv2.text((W - text_width("GAME OVER", s))//2, 48, "GAME OVER", 9, s)  # 9 = rojo
p_g, c_g, nt_g = convert(cv2)

# fusion global
alltiles = {}
merged_pat, merged_col = [], []
def merge(pairs):
    for pat, col in pairs:
        key = (tuple(pat), tuple(col))
        if key not in alltiles:
            alltiles[key] = len(merged_pat)
            merged_pat.append(pat); merged_col.append(col)

merge(zip(p_t, c_t))
merge(zip(p_g, c_g))

def remap(nt, pairs):
    local_to_key = {i: (tuple(p), tuple(c)) for i, (p, c) in enumerate(pairs)}
    return [alltiles[local_to_key[i]] for i in nt]

nt_t_g = remap(nt_t, zip(p_t, c_t))
nt_g_g = remap(nt_g, zip(p_g, c_g))

print(f"// {len(merged_pat)} tiles unicos")
flat_pat = []; flat_col = []
for p in merged_pat: flat_pat += p
for c in merged_col: flat_col += c

emit("g_TitleTiles", flat_pat)
emit("g_TitleColors", flat_col)
emit("g_TitleMap", nt_t_g)
emit("g_GameOverMap", nt_g_g)
