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
#include "font/font_mgl_sample6.h"   // symbol: g_Font_MGL_Sample6

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

#define PAC_SPEED     1
#define ENEMY_SPEED   1
#define CAM_CENTER    ((SCREEN_W / 2) - (CELL / 2)) // 120

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

u16 g_EnX, g_EnY;
u8  g_EnDir, g_EnAnim;

u8  g_Sprt, g_EnSprt;
u8  g_DigSprt[4];  // 4 sprites de digito del marcador (indices 2..5)

u16 g_CameraX;     // posicion de camara en el mundo (px), 0..MAX_SCROLL
u16 g_DrawnLeft;   // columna de tile del mundo en el borde izquierdo del name table

u8  g_Level;       // nivel actual (varia la semilla del laberinto y el tema)
u16 g_Frame;       // contador global de frames (skips de fantasma, ciclos de paleta)

u8  g_SfxTimer;    // frames restantes del SFX de comer (0 = sin sonido)
u16 g_SfxTone;     // periodo de tono actual del SFX

u8  g_HasFM;       // TRUE if any YM2413 present
u8  g_FMType;      // MSXMUSIC_NOTFOUND / _INTERNAL / _EXTERNAL

const u8 g_AllDir[4] = { DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN };

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

// Actualiza solo el PATRON de los 4 sprites de digito (su posicion es fija,
// no depende de la camara). Muestra g_Score con 4 cifras (millares..unidades).
void ShowScore()
{
	u16 v = g_Score;
	u16 div = 1000;
	for (u8 i = 0; i < 4; ++i)
	{
		u8 d = (u8)(v / div);
		v -= (u16)d * div;
		div /= 10;
		VDP_SetSpritePattern(g_DigSprt[i], SH_DIGIT + d * 4);
	}
}

//=============================================================================
// NIVEL
//=============================================================================

// Genera un nuevo nivel con mapa aleatorio y reinicia personajes. NO resetea
// la puntuacion (g_Score es acumulativa entre niveles).
void NextLevel()
{
	g_Level++;
	Math_SetRandomSeed8((u8)(0x37 + g_Level * 7));
	GenerateMaze();
	PlaceDots();
	BuildTileMap();
	ApplyTheme(g_Level);

	// Reinicia el comecocos en la celda (1,1) (siempre camino por construccion)
	g_PacX = CELL;
	g_PacY = CELL;
	g_PacDir = DIR_NONE;
	g_PacWantDir = DIR_NONE;

	// Reinicia el enemigo en una celda de camino lejana (fila impar = corredor)
	g_EnX = (MAZE_COLS - 2) * CELL;
	g_EnY = 5 * CELL;
	g_EnDir = DIR_LEFT;

	g_CameraX = 0;
	g_DrawnLeft = 0;
	InitScroll();
}

//=============================================================================
// SONIDO (PSG, canal C, no bloqueante). PSG_ACCESS == PSG_INDIRECT: las
// llamadas PSG_Set* solo escriben el buffer RAM; PSG_Apply() vuelca al chip.
//=============================================================================

// Prototipos del camino FM (definidos tras SoundUpdate)
void FM_SfxEat();
void FM_SoundUpdate();

// Dispara el efecto de "comer": tono ascendente breve en el canal C.
void SfxEat()
{
	g_SfxTimer = 6;
	if (g_HasFM)
	{
		FM_SfxEat();
		return;
	}
	g_SfxTone  = 0x0A0;
	PSG_SetTone(PSG_CHANNEL_C, g_SfxTone);
	PSG_SetVolume(PSG_CHANNEL_C, 13);
}

// Avanza el SFX un frame. SIEMPRE termina con PSG_Apply() (C1): sin el Apply
// nada del buffer RAM llega al chip en modo PSG_INDIRECT.
void SoundUpdate()
{
	if (g_HasFM)
	{
		if (g_SfxTimer > 0)
		{
			g_SfxTimer--;
			FM_SoundUpdate();
		}
		return;
	}
	if (g_SfxTimer > 0)
	{
		g_SfxTimer--;
		g_SfxTone += 0x40;
		PSG_SetTone(PSG_CHANNEL_C, g_SfxTone);
		if (g_SfxTimer == 0)
			PSG_SetVolume(PSG_CHANNEL_C, 0);
	}
	PSG_Apply();
}

//=============================================================================
// SONIDO FM (YM2413 / MSX-Music). SFX de "comer" cuando hay chip FM presente.
//=============================================================================

#define FM_BLOCK        4
#define FM_FNUM_LO      0x180
#define FM_FNUM_HI      0x210
#define FM_VOL          0

void FM_Note(u16 fnum, u8 keyOn)
{
	MSXMusic_SetRegister(0x10, (u8)(fnum & 0xFF));
	MSXMusic_SetRegister(0x20,
		(keyOn ? 0x10 : 0x00) | (FM_BLOCK << 1) | (u8)((fnum >> 8) & 0x01));
}

void FM_SfxEat()
{
	MSXMusic_SetRegister(0x30, (1 << 4) | FM_VOL); // inst 1 (Violin), loud
	FM_Note(FM_FNUM_LO, 1);
}

void FM_SoundUpdate()
{
	if (g_SfxTimer == 3)
		FM_Note(FM_FNUM_HI, 1);
	else if (g_SfxTimer == 0)
		FM_Note(FM_FNUM_HI, 0); // key-off
}

//=============================================================================
// COMECOCOS
//=============================================================================

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

		// Comer el punto de la celda actual (si lo hay)
		u16 di = (u16)cy * MAZE_COLS + cx;
		if (g_DotMap[di])
		{
			g_DotMap[di] = 0;
			g_DotsLeft--;
			g_Score++;
			SfxEat();
			u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
			// Limpia los 4 sub-tiles del punto en el mapa logico
			g_TileMap[base]                 = T_PATH;
			g_TileMap[base + 1]             = T_PATH;
			g_TileMap[base + TILE_COLS]     = T_PATH;
			g_TileMap[base + TILE_COLS + 1] = T_PATH;
			EraseDotOnScreen(cx, cy);
			ShowScore();
			if (g_DotsLeft == 0)
				NextLevel();
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
		// Ciclo de 4 fases (4 frames/fase) = un wakka completo por celda:
		// cerrado -> medio -> abierto -> medio
		u8 phase = (g_PacAnim >> 2) & 3;
		if (phase == 2)
			shape = ShapeForDir(g_PacDir);
		else if ((phase == 1) || (phase == 3))
			shape = HalfForDir(g_PacDir);
		// phase == 0 -> SH_CLOSED
	}
	VDP_SetSpritePattern(g_Sprt, shape);
	VDP_SetSpritePosition(g_Sprt, (u8)(g_PacX - g_CameraX), (u8)g_PacY);
}

//=============================================================================
// ENEMIGO (movimiento aleatorio, sin colision con el comecocos)
//=============================================================================

void UpdateEnemy()
{
	if (((g_EnX % CELL) == 0) && ((g_EnY % CELL) == 0))
	{
		u8 cx = (u8)(g_EnX / CELL);
		u8 cy = (u8)(g_EnY / CELL);
		u8 cand[4];
		u8 n = 0;
		u8 opp = Opposite(g_EnDir);
		for (u8 i = 0; i < 4; ++i)
		{
			u8 d = g_AllDir[i];
			if ((d != opp) && CanMove(cx, cy, d))
				cand[n++] = d;
		}
		if (n == 0)
		{
			for (u8 i = 0; i < 4; ++i)
			{
				u8 d = g_AllDir[i];
				if (CanMove(cx, cy, d))
					cand[n++] = d;
			}
		}
		if (n > 0)
			g_EnDir = cand[Math_GetRandomMax8(n)];
		else
			g_EnDir = DIR_NONE;
	}
	switch (g_EnDir)
	{
	case DIR_RIGHT: g_EnX += ENEMY_SPEED; break;
	case DIR_LEFT:  g_EnX -= ENEMY_SPEED; break;
	case DIR_UP:    g_EnY -= ENEMY_SPEED; break;
	case DIR_DOWN:  g_EnY += ENEMY_SPEED; break;
	}
}

void DrawEnemy()
{
	g_EnAnim++;
	i16 sx = (i16)g_EnX - (i16)g_CameraX;
	if ((sx < 0) || (sx > 255))
	{
		VDP_HideSprite(g_EnSprt);
		return;
	}
	u8 shape = ((g_EnAnim >> 3) & 1) ? (SH_GHOST + 4) : SH_GHOST;
	VDP_SetSpritePattern(g_EnSprt, shape);
	VDP_SetSpritePosition(g_EnSprt, (u8)sx, (u8)g_EnY);
}

//=============================================================================
// PANTALLA DE DEBUG (SCREEN 0) — muestra tipo de MSX y audio FM detectado
//=============================================================================

void WaitSpace()
{
	while (Keyboard_IsKeyPressed(KEY_SPACE)) { Halt(); }
	while (!Keyboard_IsKeyPressed(KEY_SPACE)) { Halt(); }
	while (Keyboard_IsKeyPressed(KEY_SPACE)) { Halt(); }
}

void DebugScreen()
{
	VDP_SetMode(VDP_MODE_SCREEN0);
	VDP_FillVRAM_16K(0, 0x0000, 0x4000);

	Print_SetTextFont(g_Font_MGL_Sample6, 1);
	Print_SetColor(0x0F, 0x00);

	Print_DrawTextAt(6, 4, "MSX COCO - DEBUG");

	Print_DrawTextAt(4, 8, "MSX TYPE: ");
	switch (Sys_GetMSXVersion())
	{
	case 0:  Print_DrawText("MSX1");     break;
	case 1:  Print_DrawText("MSX2");     break;
	case 2:  Print_DrawText("MSX2+");    break;
	case 3:  Print_DrawText("TURBO R");  break;
	default: Print_DrawText("UNKNOWN");  break;
	}

	Print_DrawTextAt(4, 10, "FM AUDIO: ");
	switch (g_FMType)
	{
	case MSXMUSIC_INTERNAL: Print_DrawText("INTERNAL"); break;
	case MSXMUSIC_EXTERNAL: Print_DrawText("FM-PAC");   break;
	default:                Print_DrawText("NONE (PSG)"); break;
	}

	Print_DrawTextAt(4, 20, "PRESS SPACE TO START");

	WaitSpace();
}

//=============================================================================
// MAIN
//=============================================================================

void main()
{
	BIOS_SetKeyClick(FALSE);

	g_FMType = MSXMusic_Initialize();
	g_HasFM  = (g_FMType != MSXMUSIC_NOTFOUND);

	DebugScreen();

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

	// Laberinto aleatorio inicial + puntos, antes de construir el tile map
	g_Level = 0;
	g_Frame = 0;
	Math_SetRandomSeed8(0x37);
	GenerateMaze();
	PlaceDots();
	BuildTileMap();

	g_PacX = CELL;
	g_PacY = CELL;
	g_PacDir = DIR_NONE;
	g_PacWantDir = DIR_NONE;
	g_PacAnim = 0;

	g_EnX = 13 * CELL;
	g_EnY = 5 * CELL;
	g_EnDir = DIR_LEFT;
	g_EnAnim = 0;

	g_Sprt = 0;
	g_EnSprt = 1;
	g_DigSprt[0] = 2;
	g_DigSprt[1] = 3;
	g_DigSprt[2] = 4;
	g_DigSprt[3] = 5;

	VDP_EnableSprite(TRUE);
	VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_1);
	VDP_LoadSpritePattern(g_PacPattern, 0, 5 * 4);
	VDP_LoadSpritePattern(g_GhostPattern, SH_GHOST, 2 * 4);
	VDP_LoadSpritePattern(g_DigitPattern, SH_DIGIT, 10 * 4);
	VDP_LoadSpritePattern(g_PacHalfPattern, SH_HALF, 4 * 4);
	VDP_SetSpriteExUniColor(g_Sprt,   (u8)g_PacX, (u8)g_PacY, SH_CLOSED, COLOR_LIGHT_YELLOW);
	VDP_SetSpriteExUniColor(g_EnSprt, (u8)g_EnX,  (u8)g_EnY,  SH_GHOST,  COLOR_LIGHT_RED);
	// Marcador: 4 digitos en posicion FIJA de pantalla (esquina superior izq.)
	VDP_SetSpriteExUniColor(g_DigSprt[0], 8,  4, SH_DIGIT, COLOR_WHITE);
	VDP_SetSpriteExUniColor(g_DigSprt[1], 24, 4, SH_DIGIT, COLOR_WHITE);
	VDP_SetSpriteExUniColor(g_DigSprt[2], 40, 4, SH_DIGIT, COLOR_WHITE);
	VDP_SetSpriteExUniColor(g_DigSprt[3], 56, 4, SH_DIGIT, COLOR_WHITE);
	VDP_DisableSpritesFrom(6);

	g_Score = 0;
	ShowScore();

	// PSG: solo el canal C activo (tono ON, ruido OFF). En modo PSG_INDIRECT
	// hay que llamar a PSG_Apply() para que el mixer/volumen lleguen al chip.
	g_SfxTimer = 0;
	PSG_SetMixer(PSG_TONE_C_ON);
	PSG_SetVolume(PSG_CHANNEL_C, 0);
	PSG_Apply();

	InitScroll();

	while (!Keyboard_IsKeyPressed(KEY_ESC))
	{
		Halt();
		// Render en V-Blank: volcado de columna entrante + scroll por hardware
		UpdateScroll();
		DrawPac();
		DrawEnemy();
		CyclePellet();
		SoundUpdate();
		// Logica para el frame siguiente
		g_Frame++;
		ReadInput();
		UpdatePac();
		UpdateEnemy();
		UpdateCamera();
	}

	BIOS_Exit(0);
}
