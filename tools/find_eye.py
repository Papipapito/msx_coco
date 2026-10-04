# Isolates the pacman sprite disc (largest contiguous yellow blob) from the raw
# capture, ignoring maze dots, and reports any transparent "eye" hole inside it.
import sys, zlib, struct

def load_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8; width=height=bitdepth=colortype=0; idat=b''; plte=b''
    while pos < len(d):
        ln = struct.unpack('>I', d[pos:pos+4])[0]
        ctype = d[pos+4:pos+8]; data = d[pos+8:pos+8+ln]
        if ctype == b'IHDR': width,height,bitdepth,colortype = struct.unpack('>IIBB', data[:10])
        elif ctype == b'PLTE': plte = data
        elif ctype == b'IDAT': idat += data
        elif ctype == b'IEND': break
        pos += 12 + ln
    raw = zlib.decompress(idat)
    ch = {0:1,2:3,3:1,4:2,6:4}[colortype]; bpp = ch; stride = width*bpp
    out = bytearray(); prev = bytearray(stride); p = 0
    def paeth(a,b,c):
        pp=a+b-c; pa,pb,pc=abs(pp-a),abs(pp-b),abs(pp-c)
        return a if (pa<=pb and pa<=pc) else (b if pb<=pc else c)
    for y in range(height):
        f = raw[p]; p += 1
        line = bytearray(raw[p:p+stride]); p += stride
        for i in range(stride):
            a = line[i-bpp] if i>=bpp else 0; b = prev[i]; c = prev[i-bpp] if i>=bpp else 0
            if f==1: line[i]=(line[i]+a)&255
            elif f==2: line[i]=(line[i]+b)&255
            elif f==3: line[i]=(line[i]+(a+b)//2)&255
            elif f==4: line[i]=(line[i]+paeth(a,b,c))&255
        out += line; prev = line
    if colortype == 3:
        rgb = bytearray(width*height*3)
        for i in range(width*height):
            idx = out[i]; rgb[i*3]=plte[idx*3]; rgb[i*3+1]=plte[idx*3+1]; rgb[i*3+2]=plte[idx*3+2]
        return width,height,3,bytes(rgb)
    return width,height,bpp,bytes(out)

def is_yellow(r,g,b): return r>180 and g>180 and b<200

w,h,bpp,px = load_png(sys.argv[1])
yel = [[False]*w for _ in range(h)]
for y in range(h):
    for x in range(w):
        o=(y*w+x)*bpp
        yel[y][x] = is_yellow(px[o],px[o+1],px[o+2])

# connected-components (4-conn) to find the largest yellow blob = pacman disc
seen = [[False]*w for _ in range(h)]
best = None
for y in range(h):
    for x in range(w):
        if yel[y][x] and not seen[y][x]:
            stack=[(y,x)]; seen[y][x]=True; cells=[]
            while stack:
                cy,cx=stack.pop(); cells.append((cy,cx))
                for dy,dx in ((1,0),(-1,0),(0,1),(0,-1)):
                    ny,nx=cy+dy,cx+dx
                    if 0<=ny<h and 0<=nx<w and yel[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx]=True; stack.append((ny,nx))
            if best is None or len(cells)>len(best): best=cells

ys=[c[0] for c in best]; xs=[c[1] for c in best]
miny,maxy,minx,maxx=min(ys),max(ys),min(xs),max(xs)
print("PACMAN disc blob: n=%d bbox x[%d-%d] y[%d-%d] size %dx%d centro(x=%d,y=%d)" % (
    len(best),minx,maxx,miny,maxy,maxx-minx+1,maxy-miny+1,(minx+maxx)//2,(miny+maxy)//2))
discset=set(best)
# render disc bbox; '.' inside bbox but not part of blob = HOLE (the eye)
holes=[]
for y in range(miny,maxy+1):
    row=""
    for x in range(minx,maxx+1):
        if (y,x) in discset: row+="#"
        else: row+="."; holes.append((y,x))
    print(row)
# the eye = interior holes (have yellow neighbours on opposite sides)
interior=[]
for (y,x) in holes:
    left = any((y,xx) in discset for xx in range(minx,x))
    right = any((y,xx) in discset for xx in range(x+1,maxx+1))
    up = any((yy,x) in discset for yy in range(miny,y))
    down = any((yy,x) in discset for yy in range(y+1,maxy+1))
    if left and right and up and down: interior.append((y,x))
if interior:
    iy=[c[0] for c in interior]; ix=[c[1] for c in interior]
    cy=(min(iy)+max(iy))//2; cx=(min(ix)+max(ix))//2
    discx=(minx+maxx)//2; discy=(miny+maxy)//2
    vpos = "upper" if cy<discy else ("lower" if cy>discy else "middle")
    hpos = "left" if cx<discx else ("right" if cx>discx else "center")
    print("EYE HOLE FOUND: %d transparent px, bbox x[%d-%d] y[%d-%d], centre(x=%d,y=%d) -> %s-%s of disc" % (
        len(interior),min(ix),max(ix),min(iy),max(iy),cx,cy,vpos,hpos))
else:
    print("NO interior hole (eye) detected in disc")
