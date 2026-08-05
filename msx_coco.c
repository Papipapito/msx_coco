// ____________________________
//  MSX COCO - Comecocos para MSX2+ (V9958) con scroll horizontal por HARDWARE
//─────────────────────────────────────────────────────────────────────────────
//  Modo: SCREEN 4 (Graphic 3). SOLO MSX2+ (usa el scroll horizontal del V9958,
//  registros R#26/R#27, inexistentes en el V9938).
//
//  Scroll: name table de 32 columnas usada de forma CIRCULAR. El laberinto es de
//  64 tiles (512 px). VDP_SetHorizontalOffset() desplaza la imagen por hardware
//  (suave, sin redibujar la pantalla); solo se vuelca la columna que entra.
//  Los sprites (comecocos/enemigo) NO se ven afectados por R#26: se posicionan
//  en pantalla como (mundo - camara).
//─────────────────────────────────────────────────────────────────────────────
#include "msxgl.h"
#include "psg.h"
#include "msx-music.h"
#include "font/font_carwar.h"        // symbol: g_Font_Carwar (chars 0x21..0x5F, ~510 B)

//=============================================================================
// DIMENSIONES
//=============================================================================

#define CELL          16
#define MAZE_COLS     32
#define MAZE_ROWS     12
#define TILE_COLS     (MAZE_COLS * 2) // 64 tiles (512 px de mundo)
#define TILE_ROWS     (MAZE_ROWS * 2) // 24 tiles
#define SCREEN_W      256
#define SCREEN_TILE_W 32              // columnas visibles (= ancho del name table)
#define MAX_SCROLL    ((TILE_COLS - SCREEN_TILE_W) * 8) // 256 px de recorrido

#define PAC_SPEED     2               // px/frame (wakka a (anim>>1)&3: 8 frames/celda)
#define GHOST_SPEED   2               // px/frame, con frame-skip (GhostMoves)
#define CAM_CENTER    ((SCREEN_W / 2) - (CELL / 2)) // 120

//=============================================================================
// ENTIDADES Y ESTADOS
//=============================================================================

#define NUM_GHOSTS    3
#define GH_RED        0               // perseguidor (Blinky)
#define GH_PINK       1               // emboscador (Pinky)
#define GH_CYAN       2               // erratico (Inky)

// Estados de fantasma
#define GST_PARKED    0               // en spawn, aun no liberado
#define GST_NORMAL    1
#define GST_FRIGHT    2
#define GST_EATEN     3               // pausa oculto en spawn tras ser comido

// FSM de juego
#define ST_TITLE      0
#define ST_READY      1
#define ST_PLAY       2
#define ST_DYING      3
#define ST_LEVELCLEAR 4
#define ST_GAMEOVER   5

// Puntos
#define PTS_DOT       1
#define PTS_PELLET    5
#define PTS_GHOST_BASE 20             // 20/40/80/160 con la cadena (cap 9999)

#define LIVES_START   3

// Mapa de sprites (indice SAT); el HUD vive en y=255 => lineas 0..15, nunca
// comparte linea fisica con pac/fantasmas (el area jugable empieza en y=16)
#define SPRT_PAC      0
#define SPRT_GHOST    1               // 1..3
#define SPRT_DIGIT    4               // 4..7 marcador
#define SPRT_LIFE     8               // 8..10 vidas
#define SPRT_LETTER   11              // 11..18 banners (READY!/GAME OVER/logo)

//=============================================================================
// TILES — bloque unico de indices (DESIGN_v020 1.1). Todo el codigo usa estos
// defines, nunca numeros magicos: reindexar aqui basta.
//=============================================================================

#define T_PATH        0
#define T_WALL        1   // +mask 0..15 -> 1..16 (bit0=N bit1=E bit2=S bit3=W; bit = vecino camino)
#define T_CNW         17  // esquina interior: diagonal NW es camino
#define T_CNE         18
#define T_CSW         19
#define T_CSE         20
// Punto centrado en la celda: 4 cuartos, uno por sub-tile, cada uno en la
// esquina que toca el CENTRO de la celda. Juntos forman un punto redondo 6x6.
#define T_DOT_TL      21  // sub-tile sup-izq -> dot en su esquina INF-DER
#define T_DOT_TR      22  // sub-tile sup-der -> dot en su esquina INF-IZQ
#define T_DOT_BL      23  // sub-tile inf-izq -> dot en su esquina SUP-DER
#define T_DOT_BR      24  // sub-tile inf-der -> dot en su esquina SUP-IZQ
// Power pellet: circulo de 12 px repartido en los 4 sub-tiles de la celda
#define T_PEL_TL      25
#define T_PEL_TR      26
#define T_PEL_BL      27
#define T_PEL_BR      28
#define NUM_TILES     29

// Mascaras de arista de pared (espacio de CELDA, no de tile)
#define WM_N          1
#define WM_E          2
#define WM_S          4
#define WM_W          8

// ---- generado por tools/genwalls.py (no editar a mano) ----
const u8 g_TilePattern[29 * 8] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0: T_PATH
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 1: T_WALL+0 (interior)
	0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 2: T_WALL+1 (N)
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, // 3: T_WALL+2 (E)
	0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, // 4: T_WALL+3 (NE)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, // 5: T_WALL+4 (S)
	0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, // 6: T_WALL+5 (NS)
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF, // 7: T_WALL+6 (ES)
	0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, 0xFF, 0xFF, // 8: T_WALL+7 (NES)
	0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, // 9: T_WALL+8 (W)
	0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, // 10: T_WALL+9 (NW)
	0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, // 11: T_WALL+10 (EW)
	0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, // 12: T_WALL+11 (NEW)
	0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF, // 13: T_WALL+12 (SW)
	0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, 0xFF, // 14: T_WALL+13 (NSW)
	0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 15: T_WALL+14 (ESW)
	0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 16: T_WALL+15 (NESW)
	0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 17: T_CNW
	0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 18: T_CNE
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0, // 19: T_CSW
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, // 20: T_CSE
	0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x07, 0x06, // 21: T_DOT_TL
	0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xE0, 0x60, // 22: T_DOT_TR
	0x06, 0x07, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, // 23: T_DOT_BL
	0x60, 0xE0, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, // 24: T_DOT_BR
	0x00, 0x00, 0x03, 0x0F, 0x1F, 0x3F, 0x3F, 0x3F, // 25: T_PEL_TL
	0x00, 0x00, 0xC0, 0xF0, 0xF8, 0xFC, 0xFC, 0xFC, // 26: T_PEL_TR
	0x3F, 0x3F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, // 27: T_PEL_BL
	0xFC, 0xFC, 0xFC, 0xF8, 0xF0, 0xC0, 0x00, 0x00, // 28: T_PEL_BR
};

const u8 g_TileColor[29 * 8] =
{
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, // 0: T_PATH
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 1: T_WALL+0 (interior)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 2: T_WALL+1 (N)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 3: T_WALL+2 (E)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 4: T_WALL+3 (NE)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 5: T_WALL+4 (S)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 6: T_WALL+5 (NS)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 7: T_WALL+6 (ES)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 8: T_WALL+7 (NES)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 9: T_WALL+8 (W)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 10: T_WALL+9 (NW)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 11: T_WALL+10 (EW)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 12: T_WALL+11 (NEW)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 13: T_WALL+12 (SW)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 14: T_WALL+13 (NSW)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 15: T_WALL+14 (ESW)
	0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, // 16: T_WALL+15 (NESW)
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, // 17: T_CNW
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, // 18: T_CNE
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, // 19: T_CSW
	0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, // 20: T_CSE
	0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, // 21: T_DOT_TL
	0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, // 22: T_DOT_TR
	0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, // 23: T_DOT_BL
	0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, // 24: T_DOT_BR
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, // 25: T_PEL_TL
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, // 26: T_PEL_TR
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, // 27: T_PEL_BL
	0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, 0x51, // 28: T_PEL_BR
};
// ---- fin del bloque generado por tools/genwalls.py ----

//=============================================================================
// PALETA — base fija + entradas temadas por nivel (DESIGN_v020 1.5)
//=============================================================================

// Entradas 1..15. Reservadas para sprites/fondo: 1 negro, 4 azul frightened,
// 7 cian Inky, 9 rojo Blinky, 11 amarillo pac, 13 rosa Pinky, 15 blanco.
// Fijas de juego: 6 dorado dots, 5 pellet (CICLADA por puerto, cero VRAM).
// Temadas por nivel: 2 relleno pared, 3 contorno, 8 acento esquinas.
const u16 g_BasePal[15] =
{
	RGB16(0, 0, 0), // 1  negro (fondo)
	RGB16(0, 0, 3), // 2  relleno pared (tema 0)
	RGB16(2, 4, 7), // 3  contorno pared (tema 0)
	RGB16(1, 1, 7), // 4  azul oscuro = frightened
	RGB16(7, 4, 1), // 5  pellet (arranca brillante)
	RGB16(6, 4, 0), // 6  dorado dots
	RGB16(2, 6, 7), // 7  cian (Inky)
	RGB16(4, 6, 7), // 8  acento esquinas (tema 0)
	RGB16(7, 3, 3), // 9  rojo claro (Blinky)
	RGB16(6, 6, 1), // 10 default MSX2 (reserva)
	RGB16(6, 6, 4), // 11 amarillo (pac)
	RGB16(1, 4, 1), // 12 default MSX2 (reserva)
	RGB16(6, 2, 5), // 13 magenta/rosa (Pinky)
	RGB16(5, 5, 5), // 14 default MSX2 (reserva)
	RGB16(7, 7, 7), // 15 blanco (HUD/letras/flash)
};

const u16 g_ThemeFill[4]   = { RGB16(0, 0, 3), RGB16(0, 2, 1), RGB16(2, 0, 1), RGB16(1, 0, 3) };
const u16 g_ThemeEdge[4]   = { RGB16(2, 4, 7), RGB16(1, 6, 3), RGB16(6, 2, 2), RGB16(4, 2, 7) };
const u16 g_ThemeAccent[4] = { RGB16(4, 6, 7), RGB16(4, 7, 5), RGB16(7, 4, 3), RGB16(6, 4, 7) };

#define PEL_BRIGHT    RGB16(7, 4, 1)
#define PEL_DIM       RGB16(3, 1, 0)

//=============================================================================
// SPRITES (16x16) — orden de cuadrantes TL,BL,TR,BR (tools/genpac.py)
//=============================================================================

#define DIR_NONE      0
#define DIR_RIGHT     1
#define DIR_LEFT      2
#define DIR_UP        3
#define DIR_DOWN      4

#define SH_CLOSED     0
#define SH_OPEN_R     4
#define SH_OPEN_L     8
#define SH_OPEN_U     12
#define SH_OPEN_D     16
#define SH_GHOST      20   // GHOST_A=20, GHOST_B=24
#define SH_DIGIT      28   // digitos 0-9 del marcador: 28 + d*4
#define SH_HALF       68   // boca media: HALF_R=68, HALF_L=72, HALF_U=76, HALF_D=80
#define SH_DEATH      84   // colapso del pac D0..D4: 84 + f*4
#define SH_FRIGHT     104  // fantasma asustado: FRIGHT_A=104, FRIGHT_B=108
#define SH_LETTER     112  // letras M,S,X,C,O,R,E,A,D,Y,G,V,!: 112 + l*4

// Indices de letra dentro de g_LetterPattern (orden de tools/genpac.py)
#define L_M           0
#define L_S           1
#define L_X           2
#define L_C           3
#define L_O           4
#define L_R           5
#define L_E           6
#define L_A           7
#define L_D           8
#define L_Y           9
#define L_G           10
#define L_V           11
#define L_EXCL        12
#define SH_L(l)       (SH_LETTER + (l) * 4)

const u8 g_PacPattern[5 * 4 * 8] =
{
	0x03, 0x0F, 0x1F, 0x3C, 0x7C, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // CLOSED
	0x03, 0x0F, 0x1F, 0x3C, 0x7C, 0x7F, 0xFC, 0xF8, 0xF8, 0xFC, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xE0, 0xF8, 0xF0, 0xC0, // OPEN_R
	0x03, 0x0F, 0x1F, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0x3C, 0x3E, 0xFE, 0x3F, 0x1F, 0x1F, 0x3F, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // OPEN_L
	0x00, 0x00, 0x00, 0x20, 0x60, 0x70, 0xF0, 0xF8, 0xE4, 0xE4, 0x7E, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x04, 0x06, 0x0E, 0x0F, 0x1F, 0x3F, 0x3F, 0x7E, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // OPEN_U
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7E, 0xFC, 0xFC, 0xF8, 0xF0, 0x70, 0x60, 0x20, 0x00, 0x00, 0x00, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0x7E, 0x27, 0x27, 0x1F, 0x0F, 0x0E, 0x06, 0x04, 0x00, 0x00, 0x00, // OPEN_D
};

const u8 g_GhostPattern[2 * 4 * 8] =
{
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x73, 0x73, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xCE, 0xCE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x39, 0x0C, 0x02, // GHOST_A
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x73, 0x73, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9C, 0x30, 0x40, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xCE, 0xCE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, // GHOST_B
};

// Digitos 0-9 del marcador (tools/gendigits.py, mismo orden de cuadrantes)
const u8 g_DigitPattern[10 * 4 * 8] =
{
	0x00, 0x0F, 0x1F, 0x38, 0x38, 0x38, 0x39, 0x3A, 0x3C, 0x38, 0x38, 0x1F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0x70, 0x70, 0xF0, 0x70, 0x70, 0x70, 0x70, 0x70, 0xE0, 0xC0, 0x00, 0x00, 0x00, // 0
	0x00, 0x07, 0x0F, 0x1F, 0x37, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0x00, 0x00, 0x00, // 1
	0x00, 0x1F, 0x3F, 0x70, 0x00, 0x01, 0x07, 0x1E, 0x78, 0x70, 0x70, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xE0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0x00, 0x00, 0x00, // 2
	0x00, 0x3F, 0x7F, 0x01, 0x03, 0x0F, 0x07, 0x00, 0x00, 0x70, 0x71, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0xC0, 0x80, 0x80, 0xC0, 0xE0, 0xE0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, // 3
	0x00, 0x03, 0x07, 0x0D, 0x19, 0x31, 0x61, 0x7F, 0x7F, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, // 4
	0x00, 0x7F, 0x7F, 0x70, 0x70, 0x7F, 0x7F, 0x00, 0x00, 0x70, 0x71, 0x3F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xE0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, // 5
	0x00, 0x0F, 0x1F, 0x39, 0x70, 0x70, 0x7F, 0x7F, 0x70, 0x70, 0x71, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xC0, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, // 6
	0x00, 0x7F, 0x7F, 0x01, 0x03, 0x03, 0x07, 0x07, 0x0E, 0x0E, 0x1C, 0x1C, 0x1C, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xE0, 0xC0, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 7
	0x00, 0x1F, 0x3F, 0x38, 0x38, 0x1F, 0x1F, 0x38, 0x70, 0x70, 0x70, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xE0, 0xE0, 0xC0, 0xC0, 0xE0, 0x70, 0x70, 0x70, 0xE0, 0xC0, 0x00, 0x00, 0x00, // 8
	0x00, 0x1F, 0x3F, 0x70, 0x70, 0x70, 0x3F, 0x1F, 0x00, 0x00, 0x71, 0x3F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0x0E, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, // 9
};

// Comecocos con boca a MEDIO abrir (4 direcciones) — fase intermedia del wakka
const u8 g_PacHalfPattern[4 * 4 * 8] =
{
	0x03, 0x0F, 0x1F, 0x3C, 0x7C, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0xFC, 0xF0, 0xE0, 0x80, 0x00, 0x00, 0x80, 0xE0, 0xF0, 0xFC, 0xF8, 0xF0, 0xC0, // HALF_R
	0x03, 0x0F, 0x1F, 0x3F, 0x0F, 0x07, 0x01, 0x00, 0x00, 0x01, 0x07, 0x0F, 0x3F, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0x3C, 0x3E, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // HALF_L
	0x00, 0x00, 0x10, 0x30, 0x78, 0x7C, 0xFC, 0xFE, 0xE7, 0xE7, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x08, 0x0C, 0x1E, 0x3E, 0x3F, 0x7F, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // HALF_U
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFE, 0xFC, 0x7C, 0x78, 0x30, 0x10, 0x00, 0x00, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE, 0xE7, 0xE7, 0x7F, 0x3F, 0x3E, 0x1E, 0x0C, 0x08, 0x00, 0x00, // HALF_D
};

// Fantasma FRIGHTENED: ojos pequenos + boca ondulada (tools/genpac.py); el
// faldon anima A/B al mismo ritmo que el patron normal
const u8 g_FrightPattern[2 * 4 * 8] =
{
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F, 0x73, 0x73, 0xFF, 0xDB, 0xB6, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE, 0xCE, 0xCE, 0xFF, 0x6D, 0xDB, 0xFF, 0xFF, 0x39, 0x0C, 0x02, // FRIGHT_A
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F, 0x73, 0x73, 0xFF, 0xDB, 0xB6, 0xFF, 0xFF, 0x9C, 0x30, 0x40, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE, 0xCE, 0xCE, 0xFF, 0x6D, 0xDB, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, // FRIGHT_B
};

// Muerte del pac: colapso hacia arriba en 5 fases (tools/genpac.py death_frames)
const u8 g_DeathPattern[5 * 4 * 8] =
{
	0x00, 0x00, 0x00, 0x20, 0x70, 0x78, 0xFC, 0xFE, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x04, 0x0E, 0x1E, 0x3F, 0x7F, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // DEATH_D0
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFE, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x7F, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // DEATH_D1
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // DEATH_D2
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xF0, 0xC0, // DEATH_D3
	0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, // DEATH_D4
};

// Letras arcade 12x12 trazo 3 px, estetica de los digitos (tools/genpac.py)
const u8 g_LetterPattern[13 * 4 * 8] =
{
	0x00, 0xE0, 0xF0, 0xF9, 0xEF, 0xEF, 0xE6, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x70, 0xF0, 0xF0, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x00, 0x00, 0x00, // LETTER_M
	0x00, 0x7F, 0xFF, 0xE0, 0xE0, 0xF0, 0x7F, 0x3F, 0x00, 0xE0, 0xE0, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xF0, 0x70, 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0x70, 0x70, 0xF0, 0xE0, 0x00, 0x00, 0x00, // LETTER_S
	0x00, 0xE0, 0xF0, 0x79, 0x3F, 0x1F, 0x0F, 0x0F, 0x1F, 0x3F, 0x79, 0xF0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x70, 0xF0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0x70, 0x00, 0x00, 0x00, // LETTER_X
	0x00, 0x3F, 0x7F, 0xF0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0x70, 0x00, 0x00, 0x00, 0x00, 0x70, 0xF0, 0xE0, 0xC0, 0x00, 0x00, 0x00, // LETTER_C
	0x00, 0x3F, 0x7F, 0xF0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xF0, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0xF0, 0xE0, 0xC0, 0x00, 0x00, 0x00, // LETTER_O
	0x00, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xFF, 0xFF, 0xE3, 0xE1, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0x70, 0xF0, 0xE0, 0xC0, 0xC0, 0xE0, 0xF0, 0x70, 0x70, 0x00, 0x00, 0x00, // LETTER_R
	0x00, 0xFF, 0xFF, 0xE0, 0xE0, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0x00, 0x00, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0x00, 0x00, 0x00, // LETTER_E
	0x00, 0x0F, 0x1F, 0x3F, 0x79, 0xE0, 0xE0, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0x70, 0x70, 0xF0, 0xF0, 0x70, 0x70, 0x70, 0x70, 0x00, 0x00, 0x00, // LETTER_A
	0x00, 0xFF, 0xFF, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x80, 0xE0, 0xF0, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0xF0, 0xE0, 0x80, 0x00, 0x00, 0x00, // LETTER_D
	0x00, 0xE0, 0xF0, 0x79, 0x3F, 0x1F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x70, 0xF0, 0xE0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // LETTER_Y
	0x00, 0x3F, 0x7F, 0xF0, 0xE0, 0xE0, 0xE3, 0xE3, 0xE0, 0xE0, 0xF0, 0x7F, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0x00, 0x00, 0xF0, 0xF0, 0x70, 0x70, 0xF0, 0xE0, 0xC0, 0x00, 0x00, 0x00, // LETTER_G
	0x00, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0x70, 0x70, 0x39, 0x39, 0x1F, 0x1F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x70, 0x70, 0x70, 0x70, 0x70, 0xE0, 0xE0, 0xC0, 0xC0, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, // LETTER_V
	0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x06, 0x00, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // LETTER_EXCL
};

//=============================================================================
// MAPA — generado en RAM por GenerateMaze() (1=pared, 0=camino)
//=============================================================================

u8 g_MazeData[MAZE_ROWS * MAZE_COLS];

//=============================================================================
// DATOS EN RAM
//=============================================================================

u8  g_TileMap[TILE_ROWS * TILE_COLS];

u8  g_DotMap[MAZE_ROWS * MAZE_COLS]; // 1 = queda punto por comer en esa celda
u16 g_DotsLeft;                      // puntos restantes en el nivel actual
u16 g_Score;                         // puntuacion acumulada entre niveles

u16 g_PacX, g_PacY;
u8  g_PacDir, g_PacWantDir, g_PacAnim;

// Fantasmas: arrays paralelos (SoA), indexados por GH_*
u16 g_GhX[NUM_GHOSTS], g_GhY[NUM_GHOSTS];
u8  g_GhDir[NUM_GHOSTS];
u8  g_GhState[NUM_GHOSTS];   // GST_*
u8  g_GhTimer[NUM_GHOSTS];   // PARKED: frames hasta liberacion / EATEN: pausa
u8  g_GhAnim;                // compartido: alterna shape A/B de los 3

// Frightened global (los pellets afectan a todos los fantasmas a la vez)
u16 g_FrightTimer;           // frames restantes; 0 = inactivo
u8  g_EatChain;              // 0..3 -> 20/40/80/160 puntos por fantasma

// Celdas de los 4 power pellets del nivel (uno por cuadrante)
u8  g_PelCX[4], g_PelCY[4];

u16 g_CameraX;     // posicion de camara en el mundo (px), 0..MAX_SCROLL
u16 g_DrawnLeft;   // columna de tile del mundo en el borde izquierdo del name table

u8  g_Level;       // nivel actual (varia la semilla del laberinto y el tema)
u16 g_Frame;       // contador global de frames (skips de fantasma, ciclos de paleta)

// Vidas / FSM / partida
u8  g_Lives;
u8  g_State;       // ST_*
u16 g_StateTimer;  // frames restantes del estado (READY/DYING/LEVELCLEAR)
u16 g_HiScore;     // persiste entre partidas hasta apagar (init UNA vez en main)
u8  g_PrevSpace;   // edge-detect de SPACE
u8  g_AltTimer;    // sub-contador reutilizable (GAMEOVER alterna / LEVELCLEAR parpadea)
u8  g_AltShow;     // toggle del sub-contador
u8  g_MazeSeedBase; // semilla de la partida (frame del SPACE = entropia humana)

// SFX (motor por tablas, un solo canal: FM ch0 o PSG C)
u8  g_SfxId;       // SFX_* activo (SFX_NONE = silencio)
u8  g_SfxFrame;    // frame actual dentro del SFX
u8  g_SfxPrio;     // prioridad del SFX activo (uno nuevo entra si prio >= actual)
u8  g_SfxBlk;      // FM: block de la ultima nota (para el key-off)
u16 g_SfxTone;     // FM: fnum de la ultima nota / PSG: periodo actual

u8  g_HasFM;       // TRUE if any YM2413 present
u8  g_FMType;      // MSXMUSIC_NOTFOUND / _INTERNAL / _EXTERNAL

const u8 g_AllDir[4] = { DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN };

// Vectores unitarios por direccion (indexados por DIR_*; DIR_NONE = (0,0))
const i8 g_DirDX[5] = { 0, 1, -1, 0, 0 };
const i8 g_DirDY[5] = { 0, 0, 0, -1, 1 };

// Spawns de fantasma (celda) y liberacion escalonada en frames tras entrar en
// PLAY. Garantizados camino por construccion: fila impar = corredor, columna
// MAZE_COLS-2 = calle lateral.
const u8 g_GhSpawnCX[NUM_GHOSTS] = { 30, 30, 30 };
const u8 g_GhSpawnCY[NUM_GHOSTS] = { 9, 5, 1 };
const u8 g_GhRelease[NUM_GHOSTS] = { 0, 120, 240 };
const u8 g_GhColor[NUM_GHOSTS]   = { COLOR_LIGHT_RED, COLOR_MAGENTA, COLOR_CYAN };

// Banners de letras-sprite (filas de <=8 sprites de 16 px: limite HW)
const u8 g_ReadyShapes[6] = { SH_L(L_R), SH_L(L_E), SH_L(L_A), SH_L(L_D), SH_L(L_Y), SH_L(L_EXCL) };
const u8 g_GameShapes[4]  = { SH_L(L_G), SH_L(L_A), SH_L(L_M), SH_L(L_E) };
const u8 g_OverShapes[4]  = { SH_L(L_O), SH_L(L_V), SH_L(L_E), SH_L(L_R) };

// Logo del titulo "MSX COCO" (SCALE_2: letra de 32 px + hueco de 16 entre palabras)
const u8 g_LogoShapes[7] = { SH_L(L_M), SH_L(L_S), SH_L(L_X), SH_L(L_C), SH_L(L_O), SH_L(L_C), SH_L(L_O) };
const u8 g_LogoX[7]      = { 8, 40, 72, 120, 152, 184, 216 };

//=============================================================================
// LABERINTO
//=============================================================================

bool IsWallCell(u8 cx, u8 cy)
{
	if ((cx >= MAZE_COLS) || (cy >= MAZE_ROWS))
		return TRUE;
	return g_MazeData[(u16)cy * MAZE_COLS + cx] != 0;
}

// Fuera de mapa = pared -> false (el borde exterior no dibuja contorno).
// Los cx-1/cy-1 con u8 envuelven a 255 y caen en el chequeo de rango.
bool IsPathCell(u8 cx, u8 cy)
{
	return !IsWallCell(cx, cy);
}

//=============================================================================
// PALETA
//=============================================================================

void InitPalette()
{
	VDP_SetPalette((const u8*)g_BasePal);
}

// Reescribe las 3 entradas temadas (relleno/contorno/acento) segun nivel % 4
void ApplyTheme(u8 level)
{
	u8 t = level & 3;
	VDP_SetPaletteEntry(2, g_ThemeFill[t]);
	VDP_SetPaletteEntry(3, g_ThemeEdge[t]);
	VDP_SetPaletteEntry(8, g_ThemeAccent[t]);
}

// Parpadeo del power pellet ciclando la entrada 5: 2 escrituras de puerto cada
// 8 frames, cero VRAM (todos los tiles de pellet comparten el indice 5)
void CyclePellet()
{
	if ((g_Frame & 7) == 0)
		VDP_SetPaletteEntry(5, (g_Frame & 8) ? PEL_BRIGHT : PEL_DIM);
}

// Genera un laberinto aleatorio con conectividad garantizada por construccion:
//  - Filas interiores impares: camino completo (corredores horizontales).
//  - Columnas 1 y MAZE_COLS-2: camino en todas las filas interiores (calles
//    laterales que unen los corredores horizontales).
//  - Filas interiores pares: pared salvo huecos aleatorios (uniones verticales).
void GenerateMaze()
{
	for (u8 cy = 0; cy < MAZE_ROWS; ++cy)
	{
		for (u8 cx = 0; cx < MAZE_COLS; ++cx)
		{
			u8 wall = 1;
			if ((cx > 0) && (cx < MAZE_COLS - 1) && (cy > 0) && (cy < MAZE_ROWS - 1))
			{
				if (cy & 1)                                   // fila impar: corredor
					wall = 0;
				else if ((cx == 1) || (cx == MAZE_COLS - 2))  // calles laterales
					wall = 0;
				else if ((Math_GetRandom8() & 3) == 0)        // hueco vertical aleatorio
					wall = 0;
			}
			g_MazeData[(u16)cy * MAZE_COLS + cx] = wall;
		}
	}
}

// Coloca un punto comestible en cada celda de camino. NO toca g_Score.
void PlaceDots()
{
	g_DotsLeft = 0;
	for (u8 cy = 0; cy < MAZE_ROWS; ++cy)
	{
		for (u8 cx = 0; cx < MAZE_COLS; ++cx)
		{
			if (IsWallCell(cx, cy))
			{
				g_DotMap[(u16)cy * MAZE_COLS + cx] = 0;
			}
			else
			{
				g_DotMap[(u16)cy * MAZE_COLS + cx] = 1;
				g_DotsLeft++;
			}
		}
	}
}

// 4 power pellets, uno por cuadrante, promocionando el dot (1->2) de la celda
// de camino mas cercana al objetivo del cuadrante. Barrido determinista por
// anillos de Chebyshev (dy exterior, dx interior, crecientes): mismo laberinto
// => mismos pellets. g_DotsLeft NO cambia: el pellet ya contaba como dot.
void PlacePellets()
{
	const i8 tgx[4] = { 2, MAZE_COLS - 3, 2, MAZE_COLS - 3 };
	const i8 tgy[4] = { 2, 2, MAZE_ROWS - 3, MAZE_ROWS - 3 };
	for (u8 q = 0; q < 4; ++q)
	{
		u8 placed = FALSE;
		for (i8 r = 0; (r <= 8) && !placed; ++r)
		{
			for (i8 dy = -r; (dy <= r) && !placed; ++dy)
			{
				for (i8 dx = -r; dx <= r; ++dx)
				{
					if ((dx > -r) && (dx < r) && (dy > -r) && (dy < r))
						continue;                  // solo el anillo exterior (Chebyshev == r)
					i8 cx = tgx[q] + dx;
					i8 cy = tgy[q] + dy;
					if ((cx < 0) || (cx >= MAZE_COLS) || (cy < 0) || (cy >= MAZE_ROWS))
						continue;
					if ((cx == 1) && (cy == 1))
						continue;                  // spawn del pac
					u16 di = (u16)(u8)cy * MAZE_COLS + (u8)cx;
					if (g_DotMap[di] != 1)
						continue;                  // pared, sin dot o ya pellet
					g_DotMap[di] = 2;
					g_PelCX[q] = (u8)cx;
					g_PelCY[q] = (u8)cy;
					placed = TRUE;
					break;
				}
			}
		}
	}
}

// Auto-tiling de paredes (DESIGN_v020 1.2/1.3): cada sub-tile de una celda de
// pared solo puede tener contorno en sus DOS lados exteriores, elegido por la
// mascara de vecinos-camino en espacio de celda. Sub-tile interior (mascara
// efectiva 0) con diagonal exterior camino -> tile de esquina. Coste: solo en
// generacion de nivel, cero por-frame.
void BuildTileMap()
{
	for (u8 cy = 0; cy < MAZE_ROWS; ++cy)
	{
		for (u8 cx = 0; cx < MAZE_COLS; ++cx)
		{
			u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
			if (!IsWallCell(cx, cy))
			{
				u8 d = g_DotMap[(u16)cy * MAZE_COLS + cx];
				u8 tl = T_PATH, tr = T_PATH, bl = T_PATH, br = T_PATH;
				if (d == 1)      { tl = T_DOT_TL; tr = T_DOT_TR; bl = T_DOT_BL; br = T_DOT_BR; }
				else if (d == 2) { tl = T_PEL_TL; tr = T_PEL_TR; bl = T_PEL_BL; br = T_PEL_BR; }
				g_TileMap[base]                 = tl;
				g_TileMap[base + 1]             = tr;
				g_TileMap[base + TILE_COLS]     = bl;
				g_TileMap[base + TILE_COLS + 1] = br;
			}
			else
			{
				u8 mN = IsPathCell(cx, cy - 1) ? WM_N : 0;
				u8 mE = IsPathCell(cx + 1, cy) ? WM_E : 0;
				u8 mS = IsPathCell(cx, cy + 1) ? WM_S : 0;
				u8 mW = IsPathCell(cx - 1, cy) ? WM_W : 0;
				u8 m;
				m = mN | mW;
				g_TileMap[base]                 = m ? (T_WALL + m) : (IsPathCell(cx - 1, cy - 1) ? T_CNW : T_WALL);
				m = mN | mE;
				g_TileMap[base + 1]             = m ? (T_WALL + m) : (IsPathCell(cx + 1, cy - 1) ? T_CNE : T_WALL);
				m = mS | mW;
				g_TileMap[base + TILE_COLS]     = m ? (T_WALL + m) : (IsPathCell(cx - 1, cy + 1) ? T_CSW : T_WALL);
				m = mS | mE;
				g_TileMap[base + TILE_COLS + 1] = m ? (T_WALL + m) : (IsPathCell(cx + 1, cy + 1) ? T_CSE : T_WALL);
			}
		}
	}
}

//=============================================================================
// SCROLL HARDWARE (V9958, name table circular)
//=============================================================================

// Vuelca al name table la columna de tiles 'worldCol' del mundo, en su posicion
// fisica circular (worldCol % 32). El name table de SCREEN 4 esta en 0x3800.
void ColumnToVRAM(u16 worldCol)
{
	u8  phys = (u8)(worldCol & (SCREEN_TILE_W - 1)); // % 32
	u16 dst  = g_ScreenLayoutLow + phys;
	u16 src  = worldCol;
	for (u8 row = 0; row < TILE_ROWS; ++row)
	{
		VDP_Poke_16K(g_TileMap[src], dst);
		dst += SCREEN_TILE_W;
		src += TILE_COLS;
	}
}

void InitScroll()
{
	g_CameraX = 0;
	g_DrawnLeft = 0;
	for (u16 c = 0; c < SCREEN_TILE_W; ++c)
		ColumnToVRAM(c);
	VDP_SetHorizontalOffset(0);
}

// Aplica la camara: PRIMERO desplaza por hardware (para que el slot fisico que
// vamos a sobrescribir quede fuera de pantalla a la derecha) y DESPUES vuelca
// las columnas que entran. Combinado con VDP_EnableMask(TRUE), elimina el
// artefacto de "bloque" en la columna izquierda durante el offset fino R#27.
void UpdateScroll()
{
	u16 newLeft = g_CameraX >> 3;
	VDP_SetHorizontalOffset(g_CameraX);
	while (g_DrawnLeft < newLeft)        // avanza a la derecha: entra columna por la derecha
	{
		ColumnToVRAM(g_DrawnLeft + SCREEN_TILE_W);
		g_DrawnLeft++;
	}
	while (g_DrawnLeft > newLeft)        // retrocede: entra columna por la izquierda
	{
		g_DrawnLeft--;
		ColumnToVRAM(g_DrawnLeft);
	}
}

void UpdateCamera()
{
	i16 target = (i16)g_PacX - CAM_CENTER;
	if (target < 0)
		target = 0;
	if (target > MAX_SCROLL)
		target = MAX_SCROLL;
	g_CameraX = (u16)target;
}

// Pone a T_PATH el sub-tile (tile-col tc, tile-fila tr) en pantalla, GUARDADO
// por la ventana del name table circular: el slot fisico tc&31 solo muestra la
// columna mundo 'tc' si tc esta visible [g_DrawnLeft, g_DrawnLeft+31]; pokear
// fuera de esa ventana corromperia la otra columna del mundo que comparte el
// slot fisico.
void ErasePathTile(u16 tc, u16 tr)
{
	if ((tc >= g_DrawnLeft) && (tc < g_DrawnLeft + SCREEN_TILE_W))
	{
		u8  phys = (u8)(tc & (SCREEN_TILE_W - 1));
		u16 dst  = g_ScreenLayoutLow + tr * SCREEN_TILE_W + phys;
		VDP_Poke_16K(T_PATH, dst);
	}
}

// Borra los 4 sub-tiles del punto de la celda (cx,cy). Cada columna del mundo
// (cx*2 y cx*2+1) puede estar dentro o fuera de la ventana de forma
// independiente, por eso cada sub-tile se guarda por SU propio tile-col (C4).
void EraseDotOnScreen(u8 cx, u8 cy)
{
	u16 tc0 = (u16)cx * 2;
	u16 tc1 = tc0 + 1;
	u16 tr0 = (u16)cy * 2;
	u16 tr1 = tr0 + 1;
	ErasePathTile(tc0, tr0);
	ErasePathTile(tc1, tr0);
	ErasePathTile(tc0, tr1);
	ErasePathTile(tc1, tr1);
}

//=============================================================================
// MOVIMIENTO COMUN
//=============================================================================

bool CanMove(u8 cx, u8 cy, u8 dir)
{
	switch (dir)
	{
	case DIR_RIGHT: cx++; break;
	case DIR_LEFT:  cx--; break;
	case DIR_UP:    cy--; break;
	case DIR_DOWN:  cy++; break;
	default: return FALSE;
	}
	return !IsWallCell(cx, cy);
}

u8 Opposite(u8 dir)
{
	switch (dir)
	{
	case DIR_RIGHT: return DIR_LEFT;
	case DIR_LEFT:  return DIR_RIGHT;
	case DIR_UP:    return DIR_DOWN;
	case DIR_DOWN:  return DIR_UP;
	}
	return DIR_NONE;
}

//=============================================================================
// MARCADOR (4 digitos de puntuacion, sprites de posicion FIJA en pantalla)
//=============================================================================

// Muestra un valor de 4 cifras en los sprites de digito (patrones solamente;
// la posicion es fija). Compartido por marcador y hi-score (GAMEOVER alterna).
void ShowValue4(u16 v)
{
	u16 div = 1000;
	for (u8 i = 0; i < 4; ++i)
	{
		u8 d = (u8)(v / div);
		v -= (u16)d * div;
		div /= 10;
		VDP_SetSpritePattern(SPRT_DIGIT + i, SH_DIGIT + d * 4);
	}
}

// Actualiza solo el PATRON de los 4 sprites de digito (su posicion es fija,
// no depende de la camara). Muestra g_Score con 4 cifras (millares..unidades).
void ShowScore()
{
	ShowValue4(g_Score);
}

// Suma puntos con tope de 9999 (el marcador es de 4 digitos) y refresca
void AddScore(u8 pts)
{
	g_Score += pts;
	if (g_Score > 9999)
		g_Score = 9999;
	ShowScore();
}

//=============================================================================
// NIVEL
//=============================================================================

// Recoloca pac y fantasmas en sus spawns (nuevo nivel o respawn tras muerte):
// pac a (1,1), fantasmas PARKED con liberacion escalonada y colores de
// personalidad, camara a 0 (InitScroll re-vuelca las 32 columnas).
void ResetPositions()
{
	g_PacX = CELL;
	g_PacY = CELL;
	g_PacDir = DIR_NONE;
	g_PacWantDir = DIR_NONE;

	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		g_GhX[i] = (u16)g_GhSpawnCX[i] * CELL;
		g_GhY[i] = (u16)g_GhSpawnCY[i] * CELL;
		g_GhDir[i] = DIR_LEFT;
		g_GhState[i] = GST_PARKED;
		g_GhTimer[i] = g_GhRelease[i];
		VDP_SetSpriteUniColor(SPRT_GHOST + i, g_GhColor[i]);
	}

	g_FrightTimer = 0;
	g_EatChain = 0;

	UpdateCamera();
	InitScroll();
}

// Genera un nuevo nivel con mapa aleatorio y reinicia personajes. NO resetea
// la puntuacion (g_Score es acumulativa entre niveles).
void NextLevel()
{
	g_Level++;
	// Semilla = base de la partida (frame del SPACE en el titulo) + nivel:
	// cada partida tiene laberintos distintos, pero reproducibles dentro de ella
	Math_SetRandomSeed8((u8)(g_MazeSeedBase + g_Level * 7));
	GenerateMaze();
	PlaceDots();
	PlacePellets();
	BuildTileMap();
	ApplyTheme(g_Level);
	ResetPositions();
}

//=============================================================================
// SFX — motor unico frame-driven por tablas (DESIGN_v020 seccion 7).
// FM (YM2413 canal 0) si hay chip; si no, PSG canal C. PSG_ACCESS==INDIRECT:
// las PSG_Set* solo escriben el buffer RAM, PSG_Apply() vuelca al chip.
//=============================================================================

#define SFX_NONE      0
#define SFX_DOT       1   // prio 0
#define SFX_PELLET    2   // prio 1
#define SFX_EATGHOST  3   // prio 2
#define SFX_START     4   // prio 3
#define SFX_CLEAR     5   // prio 3
#define SFX_DEATH     6   // prio 4

const u8 g_SfxPrioTab[7] = { 0, 0, 1, 2, 3, 3, 4 };
// Ultimo frame de cada SFX (el paso en ese frame hace el key-off/vol 0)
const u8 g_SfxLen[7]     = { 0, 6, 12, 8, 30, 24, 40 };

// Notas FM (block, fnum): C5 (4,0x159) E5 (4,0x1B2) G5 (5,0x102) C6 (5,0x159)
// E6 (5,0x1B2). Equivalencias PSG (periodo = 1789772/(16*freq)):
// C5 0xD6, E5 0xAA, G5 0x8F, C6 0x6B, E6 0x55.

// Nota FM cruda: fnum bajo (reg 0x10) + key/block/fnum alto (reg 0x20)
void FM_Note(u8 block, u16 fnum, u8 keyOn)
{
	MSXMusic_SetRegister(0x10, (u8)(fnum & 0xFF));
	MSXMusic_SetRegister(0x20,
		(keyOn ? 0x10 : 0x00) | (block << 1) | (u8)((fnum >> 8) & 0x01));
}

// Key-on recordando block/fnum: el key-off posterior debe reescribir la misma
// nota sin el bit de key (el YM2413 no tiene "silencio" directo)
void SfxFmOn(u8 block, u16 fnum)
{
	g_SfxBlk = block;
	g_SfxTone = fnum;
	FM_Note(block, fnum, 1);
}

void SfxFmOff()
{
	FM_Note(g_SfxBlk, g_SfxTone, 0);
}

void SfxPsgOn(u16 period, u8 vol)
{
	g_SfxTone = period;
	PSG_SetTone(PSG_CHANNEL_C, period);
	PSG_SetVolume(PSG_CHANNEL_C, vol);
}

void SfxPsgOff()
{
	PSG_SetVolume(PSG_CHANNEL_C, 0);
}

// Un paso (frame f) del SFX id en el camino FM. Instrumentos: 3 piano (blips),
// 7 trompeta (fanfarrias), 10 synth (muerte). Volumen 0 = fuerte.
void SfxStepFM(u8 id, u8 f)
{
	switch (id)
	{
	case SFX_DOT:      // blip C5 -> C6
		if (f == 0)       { MSXMusic_SetRegister(0x30, (3 << 4) | 0); SfxFmOn(4, 0x159); }
		else if (f == 3)  SfxFmOn(5, 0x159);
		else if (f == 6)  SfxFmOff();
		break;
	case SFX_PELLET:   // arpegio ascendente C5-E5-G5-C6
		if (f == 0)       { MSXMusic_SetRegister(0x30, (3 << 4) | 0); SfxFmOn(4, 0x159); }
		else if (f == 3)  SfxFmOn(4, 0x1B2);
		else if (f == 6)  SfxFmOn(5, 0x102);
		else if (f == 9)  SfxFmOn(5, 0x159);
		else if (f == 12) SfxFmOff();
		break;
	case SFX_EATGHOST: // doble blip agudo C6 / E6
		if (f == 0)       { MSXMusic_SetRegister(0x30, (3 << 4) | 0); SfxFmOn(5, 0x159); }
		else if (f == 2)  SfxFmOff();
		else if (f == 4)  SfxFmOn(5, 0x1B2);
		else if (f == 8)  SfxFmOff();
		break;
	case SFX_START:    // arpegio doble en un solo canal
		if (f == 0)       { MSXMusic_SetRegister(0x30, (7 << 4) | 0); SfxFmOn(4, 0x159); }
		else if (f == 4)  SfxFmOn(5, 0x102);
		else if (f == 8)  SfxFmOn(5, 0x159);
		else if (f == 12) SfxFmOff();
		else if (f == 16) SfxFmOn(4, 0x1B2);
		else if (f == 20) SfxFmOn(5, 0x102);
		else if (f == 24) SfxFmOn(5, 0x159);
		else if (f == 30) SfxFmOff();
		break;
	case SFX_CLEAR:    // fanfarria C5-E5-G5-C6 con la ultima mantenida
		if (f == 0)       { MSXMusic_SetRegister(0x30, (7 << 4) | 0); SfxFmOn(4, 0x159); }
		else if (f == 4)  SfxFmOn(4, 0x1B2);
		else if (f == 8)  SfxFmOn(5, 0x102);
		else if (f == 12) SfxFmOn(5, 0x159);
		else if (f == 24) SfxFmOff();
		break;
	case SFX_DEATH:    // slide descendente ~8 semitonos (2 writes/frame)
		if (f == 0)       { MSXMusic_SetRegister(0x30, (10 << 4) | 0); SfxFmOn(5, 0x1B2); }
		else if (f < 32)  { g_SfxTone -= 0x0C; FM_Note(5, g_SfxTone, 1); }
		else if (f == 32) SfxFmOff();
		// f 33..40: silencio
		break;
	}
}

// Mismo guion en PSG canal C (fallback sin chip FM)
void SfxStepPSG(u8 id, u8 f)
{
	switch (id)
	{
	case SFX_DOT:      // tono ascendente breve (el de la v1, conservado)
		if (f == 0)       SfxPsgOn(0x0A0, 13);
		else if (f < 6)   { g_SfxTone += 0x40; PSG_SetTone(PSG_CHANNEL_C, g_SfxTone); }
		else              SfxPsgOff();
		break;
	case SFX_PELLET:
		if (f == 0)       SfxPsgOn(0xD6, 13);
		else if (f == 3)  PSG_SetTone(PSG_CHANNEL_C, 0xAA);
		else if (f == 6)  PSG_SetTone(PSG_CHANNEL_C, 0x8F);
		else if (f == 9)  PSG_SetTone(PSG_CHANNEL_C, 0x6B);
		else if (f == 12) SfxPsgOff();
		break;
	case SFX_EATGHOST:
		if (f == 0)       SfxPsgOn(0x6B, 13);
		else if (f == 2)  SfxPsgOff();
		else if (f == 4)  SfxPsgOn(0x55, 13);
		else if (f == 8)  SfxPsgOff();
		break;
	case SFX_START:
		if (f == 0)       SfxPsgOn(0xD6, 13);
		else if (f == 4)  PSG_SetTone(PSG_CHANNEL_C, 0x8F);
		else if (f == 8)  PSG_SetTone(PSG_CHANNEL_C, 0x6B);
		else if (f == 12) SfxPsgOff();
		else if (f == 16) SfxPsgOn(0xAA, 13);
		else if (f == 20) PSG_SetTone(PSG_CHANNEL_C, 0x8F);
		else if (f == 24) PSG_SetTone(PSG_CHANNEL_C, 0x6B);
		else if (f == 30) SfxPsgOff();
		break;
	case SFX_CLEAR:
		if (f == 0)       SfxPsgOn(0xD6, 13);
		else if (f == 4)  PSG_SetTone(PSG_CHANNEL_C, 0xAA);
		else if (f == 8)  PSG_SetTone(PSG_CHANNEL_C, 0x8F);
		else if (f == 12) PSG_SetTone(PSG_CHANNEL_C, 0x6B);
		else if (f == 24) SfxPsgOff();
		break;
	case SFX_DEATH:    // periodo creciente (pitch cae), volumen desvaneciendose
		if (f == 0)       SfxPsgOn(0x55, 12);
		else if (f < 40)
		{
			g_SfxTone += 6;
			PSG_SetTone(PSG_CHANNEL_C, g_SfxTone);
			if ((f & 3) == 0)
				PSG_SetVolume(PSG_CHANNEL_C, 12 - (f >> 2));
		}
		else              SfxPsgOff();
		break;
	}
}

// Dispara un SFX: entra si no suena nada o si su prioridad iguala o supera la
// del activo (igual prioridad re-dispara: p.ej. dots encadenados)
void SfxPlay(u8 id)
{
	if ((g_SfxId == SFX_NONE) || (g_SfxPrioTab[id] >= g_SfxPrio))
	{
		g_SfxId = id;
		g_SfxFrame = 0;
		g_SfxPrio = g_SfxPrioTab[id];
	}
}

// Avanza el SFX activo un frame. SIEMPRE acaba con PSG_Apply() si no hay FM
// (C1: en PSG_INDIRECT nada del buffer RAM llega al chip sin el Apply).
void SoundUpdate()
{
	if (g_SfxId != SFX_NONE)
	{
		if (g_HasFM)
			SfxStepFM(g_SfxId, g_SfxFrame);
		else
			SfxStepPSG(g_SfxId, g_SfxFrame);
		if (g_SfxFrame >= g_SfxLen[g_SfxId])
		{
			g_SfxId = SFX_NONE;
			g_SfxPrio = 0;
		}
		else
			g_SfxFrame++;
	}
	if (!g_HasFM)
		PSG_Apply();
}

//=============================================================================
// COMECOCOS
//=============================================================================

// Prototipos (definidas mas abajo; UpdatePac/CheckCollisions las disparan)
void StartFright();
void EnterState(u8 s);

void ReadInput()
{
	if (Keyboard_IsKeyPressed(KEY_RIGHT))      g_PacWantDir = DIR_RIGHT;
	else if (Keyboard_IsKeyPressed(KEY_LEFT))  g_PacWantDir = DIR_LEFT;
	else if (Keyboard_IsKeyPressed(KEY_UP))    g_PacWantDir = DIR_UP;
	else if (Keyboard_IsKeyPressed(KEY_DOWN))  g_PacWantDir = DIR_DOWN;
	else
	{
		u8 j = Joystick_Read(JOY_PORT_1);
		if (IS_JOY_PRESSED(j, JOY_INPUT_DIR_RIGHT))     g_PacWantDir = DIR_RIGHT;
		else if (IS_JOY_PRESSED(j, JOY_INPUT_DIR_LEFT)) g_PacWantDir = DIR_LEFT;
		else if (IS_JOY_PRESSED(j, JOY_INPUT_DIR_UP))   g_PacWantDir = DIR_UP;
		else if (IS_JOY_PRESSED(j, JOY_INPUT_DIR_DOWN)) g_PacWantDir = DIR_DOWN;
	}
}

void UpdatePac()
{
	if (((g_PacX % CELL) == 0) && ((g_PacY % CELL) == 0))
	{
		u8 cx = (u8)(g_PacX / CELL);
		u8 cy = (u8)(g_PacY / CELL);

		// Comer el comestible de la celda actual (1 = dot, 2 = power pellet)
		u16 di = (u16)cy * MAZE_COLS + cx;
		u8 d = g_DotMap[di];
		if (d)
		{
			g_DotMap[di] = 0;
			g_DotsLeft--;
			u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
			// Limpia los 4 sub-tiles del comestible en el mapa logico
			g_TileMap[base]                 = T_PATH;
			g_TileMap[base + 1]             = T_PATH;
			g_TileMap[base + TILE_COLS]     = T_PATH;
			g_TileMap[base + TILE_COLS + 1] = T_PATH;
			EraseDotOnScreen(cx, cy);
			if (d == 1)
			{
				AddScore(PTS_DOT);
				SfxPlay(SFX_DOT);
			}
			else
			{
				AddScore(PTS_PELLET);
				StartFright();
				SfxPlay(SFX_PELLET);
			}
			if (g_DotsLeft == 0)
				EnterState(ST_LEVELCLEAR);
		}

		if ((g_PacWantDir != DIR_NONE) && CanMove(cx, cy, g_PacWantDir))
			g_PacDir = g_PacWantDir;
		if ((g_PacDir != DIR_NONE) && !CanMove(cx, cy, g_PacDir))
			g_PacDir = DIR_NONE;
	}
	switch (g_PacDir)
	{
	case DIR_RIGHT: g_PacX += PAC_SPEED; break;
	case DIR_LEFT:  g_PacX -= PAC_SPEED; break;
	case DIR_UP:    g_PacY -= PAC_SPEED; break;
	case DIR_DOWN:  g_PacY += PAC_SPEED; break;
	}
}

u8 ShapeForDir(u8 dir)
{
	switch (dir)
	{
	case DIR_RIGHT: return SH_OPEN_R;
	case DIR_LEFT:  return SH_OPEN_L;
	case DIR_UP:    return SH_OPEN_U;
	case DIR_DOWN:  return SH_OPEN_D;
	}
	return SH_CLOSED;
}

// Indice del patron de boca media para la direccion (mismo orden que genpac:
// HALF_R=0, HALF_L=1, HALF_U=2, HALF_D=3).
u8 HalfForDir(u8 dir)
{
	switch (dir)
	{
	case DIR_RIGHT: return SH_HALF;
	case DIR_LEFT:  return SH_HALF + 4;
	case DIR_UP:    return SH_HALF + 8;
	case DIR_DOWN:  return SH_HALF + 12;
	}
	return SH_CLOSED;
}

void DrawPac()
{
	g_PacAnim++;
	u8 shape = SH_CLOSED;
	if (g_PacDir != DIR_NONE)
	{
		// Ciclo de 4 fases (2 frames/fase a PAC_SPEED=2) = un wakka completo
		// por celda: cerrado -> medio -> abierto -> medio
		u8 phase = (g_PacAnim >> 1) & 3;
		if (phase == 2)
			shape = ShapeForDir(g_PacDir);
		else if ((phase == 1) || (phase == 3))
			shape = HalfForDir(g_PacDir);
		// phase == 0 -> SH_CLOSED
	}
	VDP_SetSpritePattern(SPRT_PAC, shape);
	VDP_SetSpritePosition(SPRT_PAC, (u8)(g_PacX - g_CameraX), (u8)g_PacY);
}

//=============================================================================
// FANTASMAS: IA por personalidad, liberacion escalonada y frame-skip
//=============================================================================

// Distancia manhattan en espacio de celda (rangos <= 32+12: cabe en u8)
u8 CellDist(i8 x, i8 y, i8 tx, i8 ty)
{
	i8 dx = x - tx;
	if (dx < 0)
		dx = -dx;
	i8 dy = y - ty;
	if (dy < 0)
		dy = -dy;
	return (u8)dx + (u8)dy;
}

// Frame-skip que mantiene la alineacion %16: el paso es SIEMPRE de 2 px y el
// skip omite el frame entero, asi que la fase de celda no se corrompe nunca.
bool GhostMoves(u8 i)
{
	if (g_GhState[i] == GST_FRIGHT)
		return (g_Frame & 1) == 0;          // 50%
	if (g_Level >= 5)
		return TRUE;                        // 100%
	if (g_Level >= 3)
		return (g_Frame & 15) != 15;        // ~94%
	return (g_Frame & 7) != 7;              // ~87.5% (base)
}

// Eleccion de direccion en celda alineada. Candidatos: != opuesta y con paso
// libre; en callejon se permite la vuelta atras (igual que el enemigo v1).
void ChooseDir(u8 i)
{
	u8 cx = (u8)(g_GhX[i] / CELL);
	u8 cy = (u8)(g_GhY[i] / CELL);
	u8 cand[4];
	u8 n = 0;
	u8 opp = Opposite(g_GhDir[i]);
	for (u8 k = 0; k < 4; ++k)
	{
		u8 d = g_AllDir[k];
		if ((d != opp) && CanMove(cx, cy, d))
			cand[n++] = d;
	}
	if (n == 0)
	{
		for (u8 k = 0; k < 4; ++k)
		{
			u8 d = g_AllDir[k];
			if (CanMove(cx, cy, d))
				cand[n++] = d;
		}
		if (n == 0)
		{
			g_GhDir[i] = DIR_NONE;
			return;
		}
	}

	if ((g_GhState[i] != GST_FRIGHT) && (i == GH_CYAN))
	{
		// Erratico: candidata aleatoria
		g_GhDir[i] = cand[Math_GetRandomMax8(n)];
		return;
	}

	// Celda objetivo segun personalidad (frightened: huir del pac)
	i8 tx = (i8)(g_PacX / CELL);
	i8 ty = (i8)(g_PacY / CELL);
	if ((g_GhState[i] != GST_FRIGHT) && (i == GH_PINK))
	{
		// Emboscador: 4 celdas por delante del pac (parado: el propio pac)
		tx += 4 * g_DirDX[g_PacDir];
		ty += 4 * g_DirDY[g_PacDir];
		if (tx < 0) tx = 0; else if (tx > MAZE_COLS - 1) tx = MAZE_COLS - 1;
		if (ty < 0) ty = 0; else if (ty > MAZE_ROWS - 1) ty = MAZE_ROWS - 1;
	}

	u8 dist[4];
	for (u8 k = 0; k < n; ++k)
	{
		u8 d = cand[k];
		dist[k] = CellDist((i8)cx + g_DirDX[d], (i8)cy + g_DirDY[d], tx, ty);
	}

	if (g_GhState[i] == GST_FRIGHT)
	{
		// MAXIMIZA la distancia; empate -> aleatoria entre las empatadas
		u8 best = 0;
		for (u8 k = 1; k < n; ++k)
			if (dist[k] > dist[best])
				best = k;
		u8 tied[4];
		u8 ties = 0;
		for (u8 k = 0; k < n; ++k)
			if (dist[k] == dist[best])
				tied[ties++] = k;
		g_GhDir[i] = cand[tied[Math_GetRandomMax8(ties)]];
	}
	else
	{
		// MINIMIZA; el primer candidato del orden fijo gana (determinista)
		u8 best = 0;
		for (u8 k = 1; k < n; ++k)
			if (dist[k] < dist[best])
				best = k;
		g_GhDir[i] = cand[best];
	}
}

void UpdateGhosts()
{
	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		if (g_GhState[i] == GST_PARKED)
		{
			// Visible y quieto en spawn hasta que expire su liberacion
			if (g_GhTimer[i] > 0)
			{
				g_GhTimer[i]--;
				continue;
			}
			g_GhState[i] = GST_NORMAL;
			g_GhDir[i] = DIR_LEFT;
		}
		else if (g_GhState[i] == GST_EATEN)
		{
			// Pausa oculto en spawn; al expirar reaparece NORMAL aunque el
			// fright global siga activo (regla clasica y mas simple)
			if (g_GhTimer[i] > 0)
			{
				g_GhTimer[i]--;
				continue;
			}
			g_GhState[i] = GST_NORMAL;
			g_GhDir[i] = DIR_LEFT;
			VDP_SetSpriteUniColor(SPRT_GHOST + i, g_GhColor[i]);
		}
		if (!GhostMoves(i))
			continue;
		// ChooseDir SOLO en frames en que se mueve: evita re-rolls del
		// erratico parado en celda alineada (la posicion no cambia si no anda)
		if (((g_GhX[i] % CELL) == 0) && ((g_GhY[i] % CELL) == 0))
			ChooseDir(i);
		switch (g_GhDir[i])
		{
		case DIR_RIGHT: g_GhX[i] += GHOST_SPEED; break;
		case DIR_LEFT:  g_GhX[i] -= GHOST_SPEED; break;
		case DIR_UP:    g_GhY[i] -= GHOST_SPEED; break;
		case DIR_DOWN:  g_GhY[i] += GHOST_SPEED; break;
		}
	}
}

void DrawGhosts()
{
	g_GhAnim++;
	u8 alt = (g_GhAnim >> 3) & 1;
	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		if (g_GhState[i] == GST_EATEN)
			continue;                    // oculto (ya escondido al comerlo)
		i16 sx = (i16)g_GhX[i] - (i16)g_CameraX;
		if ((sx < 0) || (sx > 255))
		{
			// Fuera de la ventana horizontal visible: ocultar (no envolver)
			VDP_HideSprite(SPRT_GHOST + i);
			continue;
		}
		u8 base = (g_GhState[i] == GST_FRIGHT) ? SH_FRIGHT : SH_GHOST;
		VDP_SetSpritePattern(SPRT_GHOST + i, alt ? (base + 4) : base);
		VDP_SetSpritePosition(SPRT_GHOST + i, (u8)sx, (u8)g_GhY[i]);
	}
}

//=============================================================================
// FRIGHTENED Y COLISIONES
//=============================================================================

// Comer un power pellet: todos los NORMAL dan la vuelta y pasan a FRIGHT.
// PARKED/EATEN se quedan como estan (nunca se asustan en el spawn).
void StartFright()
{
	i16 t = 420 - (i16)g_Level * 30;   // 7 s decreciente por nivel, minimo 2 s
	if (t < 120)
		t = 120;
	g_FrightTimer = (u16)t;
	g_EatChain = 0;
	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		if (g_GhState[i] == GST_NORMAL)
		{
			g_GhDir[i] = Opposite(g_GhDir[i]);
			g_GhState[i] = GST_FRIGHT;
		}
		// Re-azular tambien a los que ya eran FRIGHT: podian estar blancos
		// del parpadeo de aviso y el timer acaba de reiniciarse
		if (g_GhState[i] == GST_FRIGHT)
			VDP_SetSpriteUniColor(SPRT_GHOST + i, COLOR_DARK_BLUE);
	}
}

// Ciclo de vida del frightened: cuenta atras global, parpadeo de aviso los
// ultimos 2 s (toggle de color solo cada 16 frames: 48 B de VRAM, no cada
// frame) y vuelta a NORMAL al expirar.
void UpdateFright()
{
	if (g_FrightTimer == 0)
		return;
	g_FrightTimer--;
	if (g_FrightTimer == 0)
	{
		for (u8 i = 0; i < NUM_GHOSTS; ++i)
		{
			if (g_GhState[i] == GST_FRIGHT)
			{
				g_GhState[i] = GST_NORMAL;
				VDP_SetSpriteUniColor(SPRT_GHOST + i, g_GhColor[i]);
			}
		}
	}
	else if ((g_FrightTimer < 120) && ((g_FrightTimer & 15) == 0))
	{
		u8 c = ((g_FrightTimer >> 4) & 1) ? COLOR_WHITE : COLOR_DARK_BLUE;
		for (u8 i = 0; i < NUM_GHOSTS; ++i)
		{
			if (g_GhState[i] == GST_FRIGHT)
				VDP_SetSpriteUniColor(SPRT_GHOST + i, c);
		}
	}
}

// AABB perdonador de 12 px (los sprites son de 16): roce leve no mata.
// PARKED SI colisiona (esta en el mapa); EATEN no tiene cuerpo.
void CheckCollisions()
{
	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		if (g_GhState[i] == GST_EATEN)
			continue;
		i16 dx = (i16)g_PacX - (i16)g_GhX[i];
		if (dx < 0)
			dx = -dx;
		if (dx >= 12)
			continue;
		i16 dy = (i16)g_PacY - (i16)g_GhY[i];
		if (dy < 0)
			dy = -dy;
		if (dy >= 12)
			continue;
		if (g_GhState[i] == GST_FRIGHT)
		{
			// Comerselo: cadena 20/40/80/160 y pausa oculto en su spawn
			AddScore(PTS_GHOST_BASE << g_EatChain);
			if (g_EatChain < 3)
				g_EatChain++;
			g_GhState[i] = GST_EATEN;
			g_GhTimer[i] = 120;
			VDP_HideSprite(SPRT_GHOST + i);
			g_GhX[i] = (u16)g_GhSpawnCX[i] * CELL;
			g_GhY[i] = (u16)g_GhSpawnCY[i] * CELL;
			SfxPlay(SFX_EATGHOST);
		}
		else
		{
			// Muerte del pac: una sola por frame, congela el resto del chequeo
			EnterState(ST_DYING);
			return;
		}
	}
}

//=============================================================================
// HUD Y BANNERS DE LETRAS
//=============================================================================

void HideSprites(u8 from, u8 to)
{
	for (u8 s = from; s <= to; ++s)
		VDP_HideSprite(s);
}

// Fila de letras-sprite blancas equiespaciadas cada 16 px
void ShowLetterRow(u8 sprt, const u8* shapes, u8 n, u8 x, u8 y)
{
	for (u8 i = 0; i < n; ++i)
	{
		VDP_SetSpriteExUniColor(sprt + i, x, y, shapes[i], COLOR_WHITE);
		x += 16;
	}
}

// Vidas restantes como mini-pacs. En y=255 el sprite ocupa las lineas 0..15.
void ShowLives()
{
	for (u8 i = 0; i < 3; ++i)
	{
		if (i < g_Lives)
			VDP_SetSpriteExUniColor(SPRT_LIFE + i, 200 + i * 16, 255, SH_OPEN_R, COLOR_LIGHT_YELLOW);
		else
			VDP_HideSprite(SPRT_LIFE + i);
	}
}

// HUD completo en y=255: nunca comparte linea fisica con las entidades
// (el area jugable empieza en y=16), asi el peor caso queda en 7 sprites/linea
void ShowHUD()
{
	for (u8 i = 0; i < 4; ++i)
		VDP_SetSpriteExUniColor(SPRT_DIGIT + i, 8 + i * 16, 255, SH_DIGIT, COLOR_WHITE);
	ShowScore();
	ShowLives();
}

//=============================================================================
// TITULO (attract mode)
//=============================================================================

// Fila de tiles del name table a T_PATH: banda limpia para el texto del titulo
// (PRINT_SKIP_SPACE deja ver el tile de fondo bajo los espacios). Solo valida
// con la camara en 0 (slot fisico == columna de mundo).
void ClearTextRow(u8 row)
{
	u16 dst = g_ScreenLayoutLow + (u16)row * SCREEN_TILE_W;
	for (u8 x = 0; x < SCREEN_TILE_W; ++x)
		VDP_Poke_16K(T_PATH, dst++);
}

// Hi-score con 4 cifras fijas en el cursor actual de Print
void PrintHiScore()
{
	c8 buf[5];
	u16 v = g_HiScore;
	u16 div = 1000;
	for (u8 i = 0; i < 4; ++i)
	{
		u8 d = (u8)(v / div);
		v -= (u16)d * div;
		div /= 10;
		buf[i] = '0' + d;
	}
	buf[4] = 0;
	Print_DrawText(buf);
}

// Desfile bajo el logo: pac + 3 fantasmas marchando a la derecha con wakka y
// alternancia A/B (con SCALE_2 son 32x32; envuelven en 256 px)
void DrawParade()
{
	g_GhAnim++;
	u8 alt = (g_GhAnim >> 3) & 1;
	u8 px = (u8)(g_Frame << 1);
	u8 phase = (u8)(g_Frame >> 1) & 3;
	u8 shape = SH_CLOSED;
	if (phase == 2)
		shape = SH_OPEN_R;
	else if (phase & 1)
		shape = SH_HALF;   // HALF_R
	VDP_SetSpritePattern(SPRT_PAC, shape);
	VDP_SetSpritePosition(SPRT_PAC, px, 140);
	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		VDP_SetSpritePattern(SPRT_GHOST + i, alt ? (SH_GHOST + 4) : SH_GHOST);
		VDP_SetSpritePosition(SPRT_GHOST + i, (u8)(px - 28 - i * 28), 140);
	}
}

//=============================================================================
// MAQUINA DE ESTADOS
//=============================================================================

void NewGame()
{
	g_Score = 0;
	g_Lives = LIVES_START;
	g_Level = 0;
	// El frame en que se pulsa SPACE es azar humano: cada partida estrena
	// laberintos (|1 evita la semilla 0, que degenera el LFSR)
	g_MazeSeedBase = (u8)(g_Frame ^ 0xA5) | 1;
	NextLevel();
	EnterState(ST_READY);
}

// Setup unico de cada estado: timers, banners y ocultaciones. Las escrituras
// VRAM de banner se hacen UNA vez aqui, no por frame.
void EnterState(u8 s)
{
	g_State = s;
	switch (s)
	{
	case ST_TITLE:
		// Laberinto de attract con semilla FIJA (tests deterministas) y tema 0
		g_CameraX = 0;
		VDP_SetHorizontalOffset(0);
		ApplyTheme(0);
		Math_SetRandomSeed8(0x37);
		GenerateMaze();
		PlaceDots();
		PlacePellets();
		BuildTileMap();
		InitScroll();
		// Banda limpia + textos por Print (SOLO aqui: camara garantizada en 0)
		ClearTextRow(12);
		ClearTextRow(14);
		Print_SetTextFont(g_Font_Carwar, 128);   // tiles 128..190: sin colision
		Print_SetColor(0xF, 0x0);
		Print_DrawTextAt(9, 12, "PUSH SPACE KEY");
		Print_DrawTextAt(9, 14, "HI-SCORE ");
		PrintHiScore();
		if (g_HasFM)
			Print_DrawTextAt(28, 22, "FM");      // diagnostico barato del YM2413
		// Logo con letras-sprite a doble escala. El flag MAG es GLOBAL (R#1):
		// el HUD se oculta y el desfile tambien queda a 32x32 (deseado)
		HideSprites(SPRT_DIGIT, SPRT_LIFE + 2);
		VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_2);
		for (u8 i = 0; i < 7; ++i)
			VDP_SetSpriteExUniColor(SPRT_LETTER + i, g_LogoX[i], 48, g_LogoShapes[i], COLOR_WHITE);
		VDP_HideSprite(SPRT_LETTER + 7);         // resto de OVER si venimos de GAMEOVER
		for (u8 i = 0; i < NUM_GHOSTS; ++i)
			VDP_SetSpriteUniColor(SPRT_GHOST + i, g_GhColor[i]);
		break;

	case ST_READY:
		g_StateTimer = 120;
		ShowLetterRow(SPRT_LETTER, g_ReadyShapes, 6, 88, 100);
		// READY! solo usa 6 letras: esconder los restos de OVER (sprites 17-18)
		HideSprites(SPRT_LETTER + 6, SPRT_LETTER + 7);
		ShowHUD();
		SfxPlay(SFX_START);
		break;

	case ST_PLAY:
		HideSprites(SPRT_LETTER, SPRT_LETTER + 7);
		break;

	case ST_DYING:
		g_StateTimer = 120;
		SfxPlay(SFX_DEATH);
		break;

	case ST_LEVELCLEAR:
		g_StateTimer = 90;
		g_AltTimer = 15;
		g_AltShow = 0;
		if (g_Score > g_HiScore)
			g_HiScore = g_Score;
		SfxPlay(SFX_CLEAR);
		break;

	case ST_GAMEOVER:
		if (g_Score > g_HiScore)
			g_HiScore = g_Score;
		g_StateTimer = 0;      // cuenta HACIA ARRIBA (minimo 60 antes de aceptar SPACE)
		g_AltTimer = 90;
		g_AltShow = 0;
		VDP_HideSprite(SPRT_PAC);
		HideSprites(SPRT_GHOST, SPRT_GHOST + NUM_GHOSTS - 1);
		HideSprites(SPRT_LIFE, SPRT_LIFE + 2);
		// GAME y OVER en DOS filas: 4 sprites por linea, lejos del limite de 8
		ShowLetterRow(SPRT_LETTER, g_GameShapes, 4, 96, 88);
		ShowLetterRow(SPRT_LETTER + 4, g_OverShapes, 4, 96, 112);
		break;
	}
}

// Guion visual de la muerte (t = frames restantes): 120..61 todo congelado,
// en 60 se ocultan los fantasmas, 60..21 colapso D0..D4 (8 frames por shape),
// en 20 se oculta el pac, en 0 (UpdateDying) la transicion.
void DrawDying()
{
	u16 t = g_StateTimer;
	if (t == 60)
		HideSprites(SPRT_GHOST, SPRT_GHOST + NUM_GHOSTS - 1);
	else if ((t < 60) && (t > 20))
		VDP_SetSpritePattern(SPRT_PAC, SH_DEATH + ((((u8)(60 - t)) >> 3) << 2));
	else if (t == 20)
		VDP_HideSprite(SPRT_PAC);
}

void UpdateDying()
{
	if (--g_StateTimer > 0)
		return;
	g_Lives--;
	if (g_Lives == 0)
	{
		EnterState(ST_GAMEOVER);
	}
	else
	{
		ShowLives();
		ResetPositions();
		EnterState(ST_READY);
	}
}

// Parpadeo de la pared ciclando la ENTRADA 3 (contorno) blanco<->tema cada 15
// frames: solo puertos de paleta, cero VRAM. NextLevel restaura via ApplyTheme.
void UpdateLevelClear()
{
	if (--g_AltTimer == 0)
	{
		g_AltTimer = 15;
		g_AltShow ^= 1;
		VDP_SetPaletteEntry(3, g_AltShow ? RGB16(7, 7, 7) : g_ThemeEdge[g_Level & 3]);
	}
	if (--g_StateTimer == 0)
	{
		NextLevel();
		EnterState(ST_READY);
	}
}

// Con la camara congelada en posicion arbitraria NO se puede usar Print (name
// table circular desalineada): el hi-score se ensena REUTILIZANDO los mismos
// 4 digitos-sprite, alternando cada 90 frames entre score y hi-score.
void UpdateGameOver(u8 spaceEdge)
{
	if (g_StateTimer < 1000)
		g_StateTimer++;
	if (--g_AltTimer == 0)
	{
		g_AltTimer = 90;
		g_AltShow ^= 1;
		ShowValue4(g_AltShow ? g_HiScore : g_Score);
	}
	if (spaceEdge && (g_StateTimer >= 60))
		EnterState(ST_TITLE);
}

// Parpadeo de "PUSH SPACE KEY" cada 32 frames y arranque de partida
void UpdateTitle(u8 spaceEdge)
{
	if (spaceEdge)
	{
		// Restaurar la escala normal ANTES de entrar en READY; el InitScroll
		// de NewGame/NextLevel re-vuelca las 32 columnas y borra el texto solo
		VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_1);
		HideSprites(SPRT_LETTER, SPRT_LETTER + 7);
		NewGame();
		return;
	}
	if ((g_Frame & 31) == 0)
	{
		if (g_Frame & 32)
		{
			// Borrado barato: 14 pokes de T_PATH bajo el texto
			u16 dst = g_ScreenLayoutLow + 12 * SCREEN_TILE_W + 9;
			for (u8 x = 0; x < 14; ++x)
				VDP_Poke_16K(T_PATH, dst++);
		}
		else
		{
			Print_DrawTextAt(9, 12, "PUSH SPACE KEY");
		}
	}
}

//=============================================================================
// MAIN
//=============================================================================

void main()
{
	BIOS_SetKeyClick(FALSE);

	g_FMType = MSXMusic_Initialize();
	g_HasFM  = (g_FMType != MSXMUSIC_NOTFOUND);

	VDP_SetMode(VDP_MODE_GRAPHIC3);
	VDP_SetLayoutTable(0x3800);
	VDP_SetColorTable(0x2000);
	VDP_SetPatternTable(0x0000);
	VDP_SetSpritePatternTable(0x1800);
	VDP_SetSpriteAttributeTable(0x3E00);

	VDP_SetColor(0x11);
	VDP_ClearVRAM();
	VDP_EnableVBlank(TRUE);

	// Scroll horizontal del V9958 en modo de pagina simple (R#26/R#27)
	VDP_SetHorizontalMode(VDP_HSCROLL_SINGLE);
	// R#25 MAK: oculta los 8 px izq durante el offset fino R#27. Sin esta mascara
	// la columna izquierda "salta" cada 8 px porque R#26 ya se ha incrementado al
	// nuevo slot fisico (que acabamos de reescribir con el mundo de la derecha).
	VDP_EnableMask(TRUE);

	VDP_LoadPattern_GM2(g_TilePattern, NUM_TILES, 0);
	VDP_LoadColor_GM2(g_TileColor, NUM_TILES, 0);
	InitPalette();
	ApplyTheme(0);

	g_Level = 0;
	g_Frame = 0;
	g_HiScore = 0;    // UNA sola vez: persiste entre partidas hasta apagar
	g_PrevSpace = 0;
	g_PacAnim = 0;
	g_GhAnim = 0;

	VDP_EnableSprite(TRUE);
	VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_1);
	VDP_LoadSpritePattern(g_PacPattern, 0, 5 * 4);
	VDP_LoadSpritePattern(g_GhostPattern, SH_GHOST, 2 * 4);
	VDP_LoadSpritePattern(g_DigitPattern, SH_DIGIT, 10 * 4);
	VDP_LoadSpritePattern(g_PacHalfPattern, SH_HALF, 4 * 4);
	VDP_LoadSpritePattern(g_FrightPattern, SH_FRIGHT, 2 * 4);
	VDP_LoadSpritePattern(g_DeathPattern, SH_DEATH, 5 * 4);
	VDP_LoadSpritePattern(g_LetterPattern, SH_LETTER, 13 * 4);
	VDP_SetSpriteExUniColor(SPRT_PAC, (u8)CELL, (u8)CELL, SH_CLOSED, COLOR_LIGHT_YELLOW);
	for (u8 i = 0; i < NUM_GHOSTS; ++i)
	{
		VDP_SetSpriteExUniColor(SPRT_GHOST + i, 0, 0, SH_GHOST, g_GhColor[i]);
		VDP_HideSprite(SPRT_GHOST + i);   // DrawGhosts los coloca si son visibles
	}
	// VDP_ClearVRAM dejo el SAT a 0 (sprites "visibles" en (0,0)): ocultar
	// digitos/vidas/letras hasta que cada estado los ensene (y=213 no corta la cadena)
	HideSprites(SPRT_DIGIT, SPRT_LETTER + 7);
	VDP_DisableSpritesFrom(SPRT_LETTER + 8);

	// PSG: solo el canal C activo (tono ON, ruido OFF). En modo PSG_INDIRECT
	// hay que llamar a PSG_Apply() para que el mixer/volumen lleguen al chip.
	g_SfxId = SFX_NONE;
	g_SfxPrio = 0;
	PSG_SetMixer(PSG_TONE_C_ON);
	PSG_SetVolume(PSG_CHANNEL_C, 0);
	PSG_Apply();

	EnterState(ST_TITLE);

	// Bucle principal unico, frame-driven (cartucho ROM: no se sale jamas).
	// Fase VRAM justo tras el Halt (V-Blank), fase logica despues.
	while (1)
	{
		Halt();
		// ---- fase VRAM ----
		switch (g_State)
		{
		case ST_TITLE:
			DrawParade();
			break;
		case ST_READY:
		case ST_PLAY:
			UpdateScroll();
			DrawPac();
			DrawGhosts();
			CyclePellet();
			break;
		case ST_DYING:
			DrawDying();
			break;
		// LEVELCLEAR/GAMEOVER: pantalla estatica (paleta/digitos van por logica)
		}
		SoundUpdate();
		// ---- fase logica ----
		g_Frame++;
		u8 space = Keyboard_IsKeyPressed(KEY_SPACE) ? 1 : 0;
		u8 spaceEdge = (u8)(space && !g_PrevSpace);
		g_PrevSpace = space;
		switch (g_State)
		{
		case ST_TITLE:
			UpdateTitle(spaceEdge);
			break;
		case ST_READY:
			if (--g_StateTimer == 0)
				EnterState(ST_PLAY);
			break;
		case ST_PLAY:
			ReadInput();
			UpdatePac();
			if (g_State != ST_PLAY)
				break;                   // ultimo comestible -> LEVELCLEAR
			UpdateGhosts();
			UpdateFright();
			CheckCollisions();
			if (g_State != ST_PLAY)
				break;                   // colision -> DYING
			UpdateCamera();
			break;
		case ST_DYING:
			UpdateDying();
			break;
		case ST_LEVELCLEAR:
			UpdateLevelClear();
			break;
		case ST_GAMEOVER:
			UpdateGameOver(spaceEdge);
			break;
		}
	}
}
