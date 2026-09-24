# Genera:  - fichas redondas (pellet) y power-pellet, como 4 tiles de esquina (8x8)
#          - ojos del fantasma comido (sprite 16x16), orden de cuadrantes MSX
import math

def circle(radius, cx, cy):
    """Matriz 16x16 con 1 dentro del circulo de radio 'radius' centrado en (cx,cy)."""
    img = [[0]*16 for _ in range(16)]
    r2 = radius * radius
    for y in range(16):
        for x in range(16):
            dx = x - cx
            dy = y - cy
            if dx*dx + dy*dy <= r2:
                img[y][x] = 1
    return img

def to_quadrants(img):
    """Convierte 16x16 en 4 cuadrantes 8x8 (TL,BL,TR,BR) en bytes."""
    out = []
    for (r0, r1, c0, c1) in [(0,8,0,8),(8,16,0,8),(0,8,8,16),(8,16,8,16)]:
        for r in range(r0, r1):
            b = 0
            for c in range(c0, c1):
                if img[r][c]:
                    b |= (0x80 >> (c - c0))
            out.append(b)
    return out

def emit_tiles(name, radius):
    """Emite 4 tiles de esquina 8x8 que forman un circulo centrado."""
    img = circle(radius, 8.0, 8.0)
    # TL = img[0:8,0:8], TR = img[0:8,8:16], BL = img[8:16,0:8], BR = img[8:16,8:16]
    tiles = {
        "TL": [img[y][0:8] for y in range(0, 8)],
        "TR": [img[y][8:16] for y in range(0, 8)],
        "BL": [img[y][0:8] for y in range(8, 16)],
        "BR": [img[y][8:16] for y in range(8, 16)],
    }
    print(f"// {name} (radio {radius})")
    for key in ["TL", "TR", "BL", "BR"]:
        t = tiles[key]
        b = []
        for r in range(8):
            v = 0
            for c in range(8):
                if t[r][c]:
                    v |= (0x80 >> c)
            b.append(v)
        print("\t" + ", ".join("0x%02X" % x for x in b) + f", // {key}")

def emit_sprite(name, img):
    out = to_quadrants(img)
    print(f"// {name}")
    print("\t" + ", ".join("0x%02X" % x for x in out) + ",")

print("// ============ FICHAS (pellets) ============")
emit_tiles("Ficha normal", 3.5)
print()
emit_tiles("Power-pellet", 5.5)
print()

# Ojos del fantasma comido: dos ovalos blancos con pupila (hueco)
eyes = [[0]*16 for _ in range(16)]
def oval(px, py, rx, ry):
    for y in range(16):
        for x in range(16):
            if ((x-px)/rx)**2 + ((y-py)/ry)**2 <= 1.0:
                eyes[y][x] = 1
oval(5, 6, 3.0, 3.4)
oval(11, 6, 3.0, 3.4)
# pupilas (huecos) -> quitar del ovalo
def pupil(px, py, rx, ry):
    for y in range(16):
        for x in range(16):
            if ((x-px)/rx)**2 + ((y-py)/ry)**2 <= 1.0:
                eyes[y][x] = 0
pupil(5.5, 6, 1.2, 1.8)
pupil(11.5, 6, 1.2, 1.8)

print("// ============ OJOS (fantasma comido) ============")
emit_sprite("Ojos", eyes)
