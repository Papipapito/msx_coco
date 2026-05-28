# Genera el laberinto de MSX COCO (32x12 celdas) y lo valida por conectividad.
# Patron: filas impares = pasillo completo; filas pares = muro con huecos
# desfasados por fila -> mas laberintico que pilares, conectividad garantizada.
# Emite el array C g_MazeData (1=pared, 0=camino) y un volcado ASCII.
from collections import deque

COLS, ROWS = 32, 12

def build():
    g = [[1] * COLS for _ in range(ROWS)]
    for y in range(1, ROWS - 1):
        for x in range(1, COLS - 1):
            if y % 2 == 1:
                g[y][x] = 0                      # pasillo horizontal completo
            else:
                # huecos cada 5 columnas, desfasados segun la fila
                g[y][x] = 0 if ((x + (y // 2) * 2) % 5 == 0) else 1
    # abrir un par de columnas verticales largas para crear rutas mas abiertas
    for y in range(1, ROWS - 1):
        g[y][1] = 0
        g[y][COLS - 2] = 0
    return g

def connectivity_ok(g):
    # BFS desde la primera celda de camino; comprueba que alcanza todas
    start = None
    total = 0
    for y in range(ROWS):
        for x in range(COLS):
            if g[y][x] == 0:
                total += 1
                if start is None:
                    start = (x, y)
    seen = set([start])
    q = deque([start])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < COLS and 0 <= ny < ROWS and g[ny][nx] == 0 and (nx, ny) not in seen:
                seen.add((nx, ny))
                q.append((nx, ny))
    return len(seen), total

g = build()
reach, total = connectivity_ok(g)
print("// Conectividad: %d/%d celdas de camino alcanzables -> %s"
      % (reach, total, "OK" if reach == total else "FALLA (hay islas)"))
print("// ASCII (#=pared, .=camino):")
for row in g:
    print("// " + "".join('#' if c else '.' for c in row))

print("const u8 g_MazeData[%d * %d] =\n{" % (ROWS, COLS))
for y in range(ROWS):
    print("\t" + ", ".join(str(g[y][x]) for x in range(COLS)) + ",")
print("};")
