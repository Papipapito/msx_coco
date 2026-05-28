# Decodifica un PNG (zlib estandar, sin PIL) y vuelca como ASCII los pixeles
# amarillos del comecocos -> permite ver objetivamente de que lado esta la boca.
import sys, zlib, struct

def load_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    width = height = bitdepth = colortype = 0
    idat = b''
    plte = b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos+4])[0]
        ctype = d[pos+4:pos+8]
        data = d[pos+8:pos+8+ln]
        if ctype == b'IHDR':
            width, height, bitdepth, colortype = struct.unpack('>IIBB', data[:10])
        elif ctype == b'PLTE':
            plte = data
        elif ctype == b'IDAT':
            idat += data
        elif ctype == b'IEND':
            break
        pos += 12 + ln
    raw = zlib.decompress(idat)
    ch = {0:1, 2:3, 3:1, 4:2, 6:4}[colortype]
    bpp = ch  # bitdepth 8 asumido
    stride = width * bpp
    out = bytearray()
    prev = bytearray(stride)
    p = 0
    def paeth(a, b, c):
        pp = a + b - c
        pa, pb, pc = abs(pp-a), abs(pp-b), abs(pp-c)
        return a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
    for y in range(height):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        for i in range(stride):
            a = line[i-bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i-bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a+b)//2) & 255
            elif f == 4: line[i] = (line[i] + paeth(a,b,c)) & 255
        out += line
        prev = line
    if colortype == 3:  # indexada: expandir a RGB via paleta
        rgb = bytearray(width * height * 3)
        for i in range(width * height):
            idx = out[i]
            rgb[i*3] = plte[idx*3]; rgb[i*3+1] = plte[idx*3+1]; rgb[i*3+2] = plte[idx*3+2]
        return width, height, 3, bytes(rgb)
    return width, height, bpp, bytes(out)

def is_yellow(r, g, b):
    return r > 180 and g > 180 and b < 200  # comecocos = (224,224,155)

w, h, bpp, px = load_png(sys.argv[1])
# bounding box de amarillo
minx = w; miny = h; maxx = 0; maxy = 0; n = 0
for y in range(h):
    for x in range(w):
        o = (y*w + x)*bpp
        if is_yellow(px[o], px[o+1], px[o+2]):
            n += 1
            minx = min(minx, x); maxx = max(maxx, x)
            miny = min(miny, y); maxy = max(maxy, y)
if n == 0:
    cols = {}
    for y in range(h):
        for x in range(w):
            o = (y*w + x)*bpp
            c = (px[o], px[o+1], px[o+2])
            cols[c] = cols.get(c, 0) + 1
    top = sorted(cols.items(), key=lambda kv: -kv[1])[:8]
    print("%s: sin amarillo. Top colores: %s" % (sys.argv[1], top))
    sys.exit()
print("%s: amarillo n=%d bbox x[%d-%d] y[%d-%d] (centro x=%d)" % (sys.argv[1], n, minx, maxx, miny, maxy, (minx+maxx)//2))
for y in range(miny, maxy+1):
    row = ""
    for x in range(minx, maxx+1):
        o = (y*w + x)*bpp
        row += '#' if is_yellow(px[o], px[o+1], px[o+2]) else '.'
    print(row)
