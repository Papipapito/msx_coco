# Arquitectura técnica — MSX COCO

Documento de diseño interno del motor. Todo el juego está en un único fuente C
(`msx_coco.c`) sobre MSXgl. Objetivo de plataforma: **MSX2+ (V9958)**.

## Modo de vídeo: SCREEN 4 (Graphic 3)

- Modo de **tiles** (patrones) de 256×192, con **sprites mode 2** (16×16 a color).
- Tablas en VRAM:
  - Layout (name table): `0x3800`
  - Color table: `0x2000`
  - Pattern table: `0x0000`
  - Sprite pattern: `0x1800`, Sprite attribute: `0x3E00`
- Los tiles se cargan con `VDP_LoadPattern_GM2` / `VDP_LoadColor_GM2`, que replican
  el patrón en los **3 bancos** de Graphic Mode 2 (la pantalla se divide en 3
  tercios verticales, cada uno con su banco de 256 patrones).

Solo se usan 2 tiles: `T_PATH` (0, pasillo negro) y `T_WALL` (1, pared azul).

## Laberinto y colisión

- Modelo de **celda lógica de 16 px** = 2×2 tiles de 8 px, para que encaje el
  sprite del comecocos (16×16) en los pasillos.
- El mapa lógico es `g_MazeData[12][32]` (32×12 celdas = 512×192 px), `1`=pared,
  `0`=camino. Generado por `tools/genmap.py` (muros horizontales perforados) y
  **validado por BFS**: todas las celdas de camino son alcanzables (sin islas ni
  trampas).
- `BuildTileMap()` expande cada celda a 2×2 tiles en `g_TileMap[24][64]` (en RAM),
  que es a la vez la fuente para volcar a la VRAM y para consultar colisiones.
- Colisión: `IsWallCell(cx,cy)` consulta `g_MazeData`. El movimiento solo evalúa
  giro/parada cuando la entidad está **alineada a una celda** (coordenadas múltiplo
  de 16); entre celdas avanza en línea recta. Como `velocidad` (2) divide a `CELL`
  (16), siempre se llega alineado.

## Scroll horizontal por hardware (V9958)

Esta es la pieza central y la razón de exigir MSX2+.

- El mundo mide **64 tiles (512 px)**; la pantalla muestra 32 (256 px).
- Se usa el **scroll horizontal nativo del V9958**: `VDP_SetHorizontalOffset(x)`
  escribe **R#26** (desplazamiento grueso, `x>>3`) y **R#27** (ajuste fino 0–7 px).
  El VDP desplaza la imagen por hardware, suave a nivel de píxel.
- La *name table* tiene solo 32 columnas, así que se usa de forma **circular**:
  la columna del mundo `C` ocupa la posición física `C mod 32`. Cuando la cámara
  avanza y entra una columna nueva por un lado, `ColumnToVRAM()` la vuelca (24
  *pokes*, una por fila) sobre la columna que sale. **Nunca se repinta la pantalla
  entera** (a diferencia del módulo Scroll de MSXgl, que repinta ~768 bytes y
  produce micro-tirones).
- La cámara (`g_CameraX`, 0–256) sigue al comecocos centrándolo, con *clamp* en los
  bordes del mundo.
- **Los sprites no se ven afectados por R#26/R#27**, por lo que se posicionan
  directamente en pantalla como `mundo − cámara`.

> El módulo `Scroll` de MSXgl (que usa R#18 + repintado por software) se descartó:
> producía micro-tirones y, con su máscara de sprites, forzaba escala ×2 en todos
> los sprites. El scroll propio por hardware lo evita.

## Sprites (comecocos y enemigo)

- Sprites **16×16** (sprite mode 2), color uniforme por sprite
  (`VDP_SetSpriteExUniColor`).
- **Comecocos**: 5 formas — círculo cerrado + boca abierta en 4 direcciones. La boca
  alterna abierta/cerrada cada 8 frames mientras se mueve. Color amarillo claro.
- **Enemigo (fantasma)**: 2 formas (faldón animado), color rojo claro. Se mueve con
  la misma lógica de rejilla, pero en cada intersección elige dirección **al azar**
  (`Math_GetRandomMax8`) evitando dar marcha atrás salvo en callejón. Sin colisión
  con el comecocos. Se oculta (`VDP_HideSprite`) si el scroll lo deja fuera de pantalla.
- **Orden de cuadrantes**: los sprites 16×16 del MSX se almacenan column-major
  (**TL, BL, TR, BR**). `tools/genpac.py` genera los bytes en ese orden a partir de
  dibujos ASCII.

## Bucle principal

```
cada frame:
  Halt()                 # espera V-Blank
  UpdateScroll()         # vuelca columna entrante + VDP_SetHorizontalOffset (en V-Blank)
  DrawPac() / DrawEnemy() # posiciona sprites = mundo - cámara
  ReadInput()            # teclado + joystick
  UpdatePac() / UpdateEnemy() / UpdateCamera()  # lógica del frame siguiente
```

El render va justo tras `Halt()` (dentro del V-Blank) para evitar *tearing*.

## Herramientas (carpeta `tools/`)

- `genpac.py` — dibuja los sprites en ASCII y emite los arrays C (`g_PacPattern`,
  `g_GhostPattern`) en el orden de cuadrantes del MSX.
- `genmap.py` — genera el laberinto, valida conectividad por BFS y emite `g_MazeData`.
- `analyze.py` — decodifica un PNG (incl. paleta indexada) y vuelca un sprite como
  ASCII; sirve para verificar formas desde capturas de openMSX a resolución nativa.

## Compilación

- Target `ROM_32K`, `Machine = "2P"` (MSX2+). `build.sh` compila vía MSXgl/SDCC en
  WSL y copia `msx_coco.rom`.
