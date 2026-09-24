// ____________________________
//  MSX COCO - Comecocos para MSX2+ (V9958) con scroll horizontal por HARDWARE
//?????????????????????????????????????????????????????????????????????????????
//  Modo: SCREEN 4 (Graphic 3). SOLO MSX2+ (usa el scroll horizontal del V9958,
//  registros R#26/R#27, inexistentes en el V9938).
//
//  Scroll: name table de 32 columnas usada de forma CIRCULAR. El laberinto es de
//  64 tiles (512 px). VDP_SetHorizontalOffset() desplaza la imagen por hardware
//  (suave, sin redibujar la pantalla); solo se vuelca la columna que entra.
//  Los sprites (comecocos/enemigo) NO se ven afectados por R#26: se posicionan
//  en pantalla como (mundo - camara).
//
//  Juego completo: laberinto procedural, puntos comestibles y marcador, HUD con
//  vidas y puntuacion, 4 fantasmas con IA, colision, game over y sonido PSG.
//?????????????????????????????????????????????????????????????????????????????
#include "msxgl.h"
#include "psg.h"
#include "font/font_mgl_std0.h"

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

#define PAC_SPEED      2
#define GHOST_SPEED    2
#define CAM_CENTER     ((SCREEN_W / 2) - (CELL / 2)) // 120

#define LIVES_MAX      3
#define GHOST_COUNT    4

// Rejilla de "salas" para el laberinto procedural (cada sala = 1 celda 16px)
#define RW             ((MAZE_COLS - 2) / 2) // 15 salas de ancho
#define RH             ((MAZE_ROWS - 2) / 2) // 5 salas de alto

#define SCORE_DOT      10
#define SCORE_POWER    50
#define SCORE_GHOST    200
#define SCORE_DIGITS   4

#define FRIGHT_TIME    360   // fotogramas de fantasma asustado (~6 s)

//=============================================================================
// DIRECCIONES Y ESTADOS
//=============================================================================

#define DIR_NONE    0
#define DIR_RIGHT   1
#define DIR_LEFT    2
#define DIR_UP      3
#define DIR_DOWN    4

#define STATE_TITLE     0
#define STATE_PLAYING   1
#define STATE_DYING     2
#define STATE_GAMEOVER  3

#define MODE_ROAM       0
#define MODE_HUNT       1

#define GHOST_NORMAL    0
#define GHOST_FRIGHT    1
#define GHOST_EATEN     2

#define FONT_OFFSET     32   // fuente en patrones 32..125
#define TITLE_OFFSET    126  // arte de titulo/game over en 126..146

//=============================================================================
// TILES Y SPRITES
//=============================================================================

#define T_PATH   0
#define T_FILL   1
#define T_L      2
#define T_R      3
#define T_T      4
#define T_B      5
#define T_TL     6
#define T_TR     7
#define T_BL     8
#define T_BR     9
#define T_DOT_TL 10
#define T_DOT_TR 11
#define T_DOT_BL 12
#define T_DOT_BR 13
#define T_POW_TL 14
#define T_POW_TR 15
#define T_POW_BL 16
#define T_POW_BR 17
#define T_COUNT  18

#define SH_CLOSED   0
#define SH_OPEN_R   4
#define SH_OPEN_L   8
#define SH_OPEN_U   12
#define SH_OPEN_D   16
#define SH_GHOST    20   // GHOST_A=20, GHOST_B=24
#define SH_EYES     28   // ojos del fantasma comido
#define SH_DIGIT_0  32   // 10 digitos x 4 cuadrantes = patrones 32..71

#define MODE_PHASES 6

// Sprite IDs
#define SPR_PAC       0
#define SPR_GHOST     1   // 1..4
#define SPR_LIFE      5   // 5..7
#define SPR_SCORE     8   // 8..11

// Datos estaticos (tiles, sprites, digitos y configuracion de fantasmas)
#include "msx_coco_data.h"
// Arte de las pantallas de titulo y game over
#include "msx_coco_title.h"

//=============================================================================
// FANTASMAS (tipo)
//=============================================================================

typedef struct
{
	u16 x, y;
	u8  dir;
	u8  anim;
	u8  sprId;
	u8  color;
	u8  homeX, homeY;
	u8  state;   // GHOST_NORMAL / GHOST_FRIGHT / GHOST_EATEN
} Ghost;

//=============================================================================
// DATOS EN RAM
//=============================================================================

u8  g_TileMap[TILE_ROWS * TILE_COLS];
u8  g_MazeData[MAZE_ROWS * MAZE_COLS];
u8  g_Dots[MAZE_ROWS * MAZE_COLS];
u8  g_PowerDots[MAZE_ROWS * MAZE_COLS];
u8  g_PowerCell[4 * 2]; // coordenadas (x,y) de los 4 power-pellets

u8  g_RoomLink[RW * RH];
u8  g_GenVisited[RW * RH];
u8  g_GenStack[RW * RH];

u16 g_PacX, g_PacY;
u8  g_PacDir, g_PacWantDir, g_PacAnim;

Ghost g_Ghosts[GHOST_COUNT];

u16 g_Score;
u16 g_DotsLeft;
u8  g_Level;
u8  g_Lives;
u8  g_State;
u8  g_StateTimer;
u8  g_DeathTimer;
u8  g_BlinkTimer;
u8  g_BlinkOn;

u8  g_Mode;
u8  g_ModePhase;
u16 g_ModeTimer;

u16 g_FrightTimer;
u8  g_PowerBlink;
u8  g_PowerVisible;

u16 g_CameraX;
u16 g_DrawnLeft;

u16 g_SfxPeriod;
i16 g_SfxStep;
u16 g_SfxTimer;

//=============================================================================
// LABERINTO
//=============================================================================

bool IsWallCell(u8 cx, u8 cy)
{
	if ((cx >= MAZE_COLS) || (cy >= MAZE_ROWS))
		return TRUE;
	return g_MazeData[(u16)cy * MAZE_COLS + cx] != 0;
}

bool IsPathCell(u8 cx, u8 cy)
{
	return !IsWallCell(cx, cy);
}

u8 Rnd(u8 n)
{
	return (u8)(Math_GetRandom16() % n);
}

// Genera un laberinto procedural (recursive backtracker sobre 15x5 salas + lazos)
void GenMaze()
{
	Math_SetRandomSeed16(g_JIFFY);

	// 1. todo paredes, sin enlaces
	for (u16 i = 0; i < MAZE_ROWS * MAZE_COLS; ++i)
		g_MazeData[i] = 1;
	for (u16 i = 0; i < RW * RH; ++i)
	{
		g_RoomLink[i] = 0;
		g_GenVisited[i] = 0;
	}

	// 2. recursive backtracker (iterativo)
	u8 sp = 0;
	u8 cx = Rnd(RW);
	u8 cy = Rnd(RH);
	g_GenVisited[cy * RW + cx] = 1;
	g_GenStack[sp++] = cy * RW + cx;

	while (sp > 0)
	{
		u8 idx = g_GenStack[sp - 1];
		u8 x = idx % RW;
		u8 y = idx / RW;

		u8 dirs[4];
		u8 nd = 0;
		if (x + 1 < RW && !g_GenVisited[idx + 1])       dirs[nd++] = 0; // derecha
		if (y + 1 < RH && !g_GenVisited[idx + RW])      dirs[nd++] = 1; // abajo
		if (x > 0     && !g_GenVisited[idx - 1])        dirs[nd++] = 2; // izquierda
		if (y > 0     && !g_GenVisited[idx - RW])       dirs[nd++] = 3; // arriba

		if (nd > 0)
		{
			u8 d = dirs[Rnd(nd)];
			u8 nx = x, ny = y;
			switch (d)
			{
			case 0: g_RoomLink[idx] |= 1; nx++; g_RoomLink[idx + 1]   |= 4; break;
			case 1: g_RoomLink[idx] |= 2; ny++; g_RoomLink[idx + RW]  |= 8; break;
			case 2: g_RoomLink[idx] |= 4; nx--; g_RoomLink[idx - 1]   |= 1; break;
			case 3: g_RoomLink[idx] |= 8; ny--; g_RoomLink[idx - RW]  |= 2; break;
			}
			idx = ny * RW + nx;
			g_GenVisited[idx] = 1;
			g_GenStack[sp++] = idx;
		}
		else
		{
			sp--; // retroceder
		}
	}

	// 3. lazos (abre pasillos extra para dar rutas de escape)
	for (u8 i = 0; i < RW * RH / 2; ++i)
	{
		u8 x = Rnd(RW);
		u8 y = Rnd(RH);
		u8 idx = y * RW + x;
		switch (Rnd(4))
		{
		case 0: if (x + 1 < RW) { g_RoomLink[idx] |= 1; g_RoomLink[idx + 1]  |= 4; } break;
		case 1: if (y + 1 < RH) { g_RoomLink[idx] |= 2; g_RoomLink[idx + RW] |= 8; } break;
		case 2: if (x > 0)      { g_RoomLink[idx] |= 4; g_RoomLink[idx - 1]  |= 1; } break;
		case 3: if (y > 0)      { g_RoomLink[idx] |= 8; g_RoomLink[idx - RW] |= 2; } break;
		}
	}

	// 4. volcar salas y pasillos al mapa de celdas
	for (u8 ry = 0; ry < RH; ++ry)
	{
		for (u8 rx = 0; rx < RW; ++rx)
		{
			u8 cellX = rx * 2 + 1;
			u8 cellY = ry * 2 + 1;
			u8 link = g_RoomLink[ry * RW + rx];
			g_MazeData[cellY * MAZE_COLS + cellX] = 0;      // sala
			if (link & 1) g_MazeData[cellY * MAZE_COLS + cellX + 1] = 0; // pasillo derecha
			if (link & 2) g_MazeData[(cellY + 1) * MAZE_COLS + cellX] = 0; // pasillo abajo
		}
	}

	// 5. puntos en todas las celdas de camino
	g_DotsLeft = 0;
	for (u8 cy = 0; cy < MAZE_ROWS; ++cy)
	{
		for (u8 cx = 0; cx < MAZE_COLS; ++cx)
		{
			g_PowerDots[cy * MAZE_COLS + cx] = 0;
			if (IsPathCell(cx, cy))
			{
				g_Dots[cy * MAZE_COLS + cx] = 1;
				g_DotsLeft++;
			}
			else
			{
				g_Dots[cy * MAZE_COLS + cx] = 0;
			}
		}
	}

	// 6. power-pellets en 4 salas (esquinas + centro); sustituyen a la ficha
	const u8 pw[4][2] = { { RW - 1, 0 }, { 0, RH - 1 }, { RW - 1, RH - 1 }, { RW / 2, RH / 2 } };
	for (u8 k = 0; k < 4; ++k)
	{
		u8 cx = pw[k][0] * 2 + 1;
		u8 cy = pw[k][1] * 2 + 1;
		g_Dots[cy * MAZE_COLS + cx] = 0;
		g_PowerDots[cy * MAZE_COLS + cx] = 1;
		g_PowerCell[k * 2]     = cx;
		g_PowerCell[k * 2 + 1] = cy;
	}
	g_PowerVisible = TRUE;
	g_PowerBlink = 0;
	g_FrightTimer = 0;
}

void BuildTileMap()
{
	for (u8 cy = 0; cy < MAZE_ROWS; ++cy)
	{
		for (u8 cx = 0; cx < MAZE_COLS; ++cx)
		{
			u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
			if (IsWallCell(cx, cy))
			{
				bool top   = IsPathCell(cx, cy - 1);
				bool bot   = IsPathCell(cx, cy + 1);
				bool left  = IsPathCell(cx - 1, cy);
				bool right = IsPathCell(cx + 1, cy);
				g_TileMap[base]                 = (top && left)  ? T_TL : (left  ? T_L : (top ? T_T : T_FILL));
				g_TileMap[base + 1]             = (top && right) ? T_TR : (right ? T_R : (top ? T_T : T_FILL));
				g_TileMap[base + TILE_COLS]     = (bot && left)  ? T_BL : (left  ? T_L : (bot ? T_B : T_FILL));
				g_TileMap[base + TILE_COLS + 1] = (bot && right) ? T_BR : (right ? T_R : (bot ? T_B : T_FILL));
			}
			else if (g_PowerDots[cy * MAZE_COLS + cx])
			{
				g_TileMap[base]                 = T_POW_TL;
				g_TileMap[base + 1]             = T_POW_TR;
				g_TileMap[base + TILE_COLS]     = T_POW_BL;
				g_TileMap[base + TILE_COLS + 1] = T_POW_BR;
			}
			else if (g_Dots[cy * MAZE_COLS + cx])
			{
				g_TileMap[base]                 = T_DOT_TL;
				g_TileMap[base + 1]             = T_DOT_TR;
				g_TileMap[base + TILE_COLS]     = T_DOT_BL;
				g_TileMap[base + TILE_COLS + 1] = T_DOT_BR;
			}
			else
			{
				g_TileMap[base]                 = T_PATH;
				g_TileMap[base + 1]             = T_PATH;
				g_TileMap[base + TILE_COLS]     = T_PATH;
				g_TileMap[base + TILE_COLS + 1] = T_PATH;
			}
		}
	}
}

//=============================================================================
// SCROLL HARDWARE (V9958, name table circular)
//=============================================================================

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

void UpdateScroll()
{
	u16 newLeft = g_CameraX >> 3;
	while (g_DrawnLeft < newLeft)
	{
		ColumnToVRAM(g_DrawnLeft + SCREEN_TILE_W);
		g_DrawnLeft++;
	}
	while (g_DrawnLeft > newLeft)
	{
		g_DrawnLeft--;
		ColumnToVRAM(g_DrawnLeft);
	}
	VDP_SetHorizontalOffset(g_CameraX);
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

u16 Dist2(u8 x1, u8 y1, u8 x2, u8 y2)
{
	i8 dx = (i8)x1 - (i8)x2;
	i8 dy = (i8)y1 - (i8)y2;
	return (u16)(dx * dx + dy * dy);
}

void Ahead(u8 px, u8 py, u8 dir, u8 n, u8* ax, u8* ay)
{
	*ax = px;
	*ay = py;
	for (u8 i = 0; i < n; ++i)
	{
		switch (dir)
		{
		case DIR_RIGHT: (*ax)++; break;
		case DIR_LEFT:  (*ax)--; break;
		case DIR_UP:    (*ay)--; break;
		case DIR_DOWN:  (*ay)++; break;
		}
	}
}

//=============================================================================
// IA DE FANTASMAS (persecucion/difusion + eleccion greedy hacia el objetivo)
//=============================================================================

void ComputeTarget(u8 gi, u8* tx, u8* ty)
{
	u8 px = (u8)(g_PacX / CELL);
	u8 py = (u8)(g_PacY / CELL);

	if (g_Mode == MODE_ROAM)
	{
		*tx = g_GhostHomeX[gi];
		*ty = g_GhostHomeY[gi];
		return;
	}

	switch (gi)
	{
	case 0: // Blinky: persigue al comecocos
		*tx = px;
		*ty = py;
		break;
	case 1: // Pinky: apunta 4 casillas por delante
		Ahead(px, py, g_PacDir, 4, tx, ty);
		break;
	case 2: // Inky: refleja la posicion de Blinky respecto a (pac + 2)
	{
		u8 ax, ay;
		Ahead(px, py, g_PacDir, 2, &ax, &ay);
		u8 bx = (u8)(g_Ghosts[0].x / CELL);
		u8 by = (u8)(g_Ghosts[0].y / CELL);
		i16 rx = 2 * (i16)ax - (i16)bx;
		i16 ry = 2 * (i16)ay - (i16)by;
		if (rx < 0) rx = 0; else if (rx >= MAZE_COLS) rx = MAZE_COLS - 1;
		if (ry < 0) ry = 0; else if (ry >= MAZE_ROWS) ry = MAZE_ROWS - 1;
		*tx = (u8)rx;
		*ty = (u8)ry;
		break;
	}
	case 3: // Clyde: lejos persigue, cerca se dispersa
	{
		u8 cx = (u8)(g_Ghosts[3].x / CELL);
		u8 cy = (u8)(g_Ghosts[3].y / CELL);
		i8 dx = (i8)cx - (i8)px; if (dx < 0) dx = -dx;
		i8 dy = (i8)cy - (i8)py; if (dy < 0) dy = -dy;
		if ((dx + dy) > 8) { *tx = px; *ty = py; }
		else { *tx = g_GhostHomeX[3]; *ty = g_GhostHomeY[3]; }
		break;
	}
	}
}

void GhostChooseDir(u8 gi)
{
	Ghost* g = &g_Ghosts[gi];
	u8 cx = (u8)(g->x / CELL);
	u8 cy = (u8)(g->y / CELL);

	// fantasma comido: al volver a su sala se recupera
	if ((g->state == GHOST_EATEN) && (cx == (u8)(g_GhostStartX[gi] / CELL)) && (cy == (u8)(g_GhostStartY[gi] / CELL)))
		g->state = GHOST_NORMAL;

	// asustado: movimiento aleatorio (sin dar marcha atras)
	if (g->state == GHOST_FRIGHT)
	{
		u8 opp = Opposite(g->dir);
		u8 cand[4];
		u8 n = 0;
		for (u8 i = 0; i < 4; ++i)
		{
			u8 d = g_DirOrder[i];
			if ((d != opp) && CanMove(cx, cy, d))
				cand[n++] = d;
		}
		if (n == 0)
			g->dir = CanMove(cx, cy, opp) ? opp : DIR_NONE;
		else
			g->dir = cand[Rnd(n)];
		return;
	}

	// objetivo: vuelta a casa (comido) o persecucion/dispersion (normal)
	u8 tx, ty;
	if (g->state == GHOST_EATEN)
	{
		tx = (u8)(g_GhostStartX[gi] / CELL);
		ty = (u8)(g_GhostStartY[gi] / CELL);
	}
	else
	{
		ComputeTarget(gi, &tx, &ty);
	}

	u8  opp = Opposite(g->dir);
	u8  best = DIR_NONE;
	u16 bestD = 0xFFFF;

	for (u8 i = 0; i < 4; ++i)
	{
		u8 d = g_DirOrder[i];
		if (d == opp)
			continue;
		if (!CanMove(cx, cy, d))
			continue;
		u8 nx = cx, ny = cy;
		switch (d)
		{
		case DIR_RIGHT: nx++; break;
		case DIR_LEFT:  nx--; break;
		case DIR_UP:    ny--; break;
		case DIR_DOWN:  ny++; break;
		}
		u16 dist = Dist2(nx, ny, tx, ty);
		if (dist < bestD)
		{
			bestD = dist;
			best = d;
		}
	}

	if (best != DIR_NONE)
	{
		g->dir = best;
		return;
	}
	// callejon sin salida: girar
	if (CanMove(cx, cy, opp))
		g->dir = opp;
	else
		g->dir = DIR_NONE;
}

//=============================================================================
// SONIDO (PSG)
//=============================================================================

void Sound_Init()
{
	PSG_SetMixer(PSG_TONE_A_ON);
	PSG_SetVolume(PSG_CHANNEL_A, 0);
	PSG_Apply();
	g_SfxTimer = 0;
}

void Sound_Play(u16 period, u16 frames, i16 step)
{
	g_SfxPeriod = period;
	g_SfxStep = step;
	g_SfxTimer = frames;
	PSG_SetTone(PSG_CHANNEL_A, period);
	PSG_SetVolume(PSG_CHANNEL_A, 14);
}

void Sound_Update()
{
	if (g_SfxTimer)
	{
		g_SfxTimer--;
		if (g_SfxStep != 0)
		{
			g_SfxPeriod = (u16)((i16)g_SfxPeriod + g_SfxStep);
			PSG_SetTone(PSG_CHANNEL_A, g_SfxPeriod);
		}
		if (g_SfxTimer == 0)
			PSG_SetVolume(PSG_CHANNEL_A, 0);
	}
	PSG_Apply();
}

//=============================================================================
// PALETA
//=============================================================================

void SetPalette()
{
	VDP_SetPaletteEntry(1,  RGB16(0, 0, 0)); // negro
	VDP_SetPaletteEntry(2,  RGB16(0, 5, 0)); // verde
	VDP_SetPaletteEntry(3,  RGB16(0, 7, 0)); // verde claro
	VDP_SetPaletteEntry(4,  RGB16(0, 0, 5)); // azul oscuro (borde muro)
	VDP_SetPaletteEntry(5,  RGB16(0, 4, 7)); // azul claro (relleno muro)
	VDP_SetPaletteEntry(6,  RGB16(4, 0, 0)); // rojo oscuro
	VDP_SetPaletteEntry(7,  RGB16(0, 7, 7)); // cian
	VDP_SetPaletteEntry(8,  RGB16(6, 0, 0)); // rojo medio
	VDP_SetPaletteEntry(9,  RGB16(7, 0, 0)); // rojo claro
	VDP_SetPaletteEntry(10, RGB16(7, 4, 0)); // naranja
	VDP_SetPaletteEntry(11, RGB16(7, 7, 0)); // amarillo
	VDP_SetPaletteEntry(12, RGB16(0, 4, 0)); // verde oscuro
	VDP_SetPaletteEntry(13, RGB16(7, 0, 7)); // magenta
	VDP_SetPaletteEntry(14, RGB16(4, 4, 4)); // gris
	VDP_SetPaletteEntry(15, RGB16(7, 7, 7)); // blanco
}

//=============================================================================
// ENTRADA
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

bool StartPressed()
{
	return Keyboard_IsKeyPressed(KEY_SPACE)
		|| Joystick_IsButtonPressed(JOY_PORT_1, JOY_INPUT_TRIGGER_A);
}

//=============================================================================
// COMECOCOS
//=============================================================================

// Pone los 4 tiles de la celda a T_PATH (en RAM y VRAM)
void ClearCell(u8 cx, u8 cy)
{
	u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
	g_TileMap[base]                 = T_PATH;
	g_TileMap[base + 1]             = T_PATH;
	g_TileMap[base + TILE_COLS]     = T_PATH;
	g_TileMap[base + TILE_COLS + 1] = T_PATH;

	u8 tx = cx * 2;
	u8 ty = cy * 2;
	VDP_Poke_16K(T_PATH, g_ScreenLayoutLow + (u16)ty * 32 + (tx & 31));
	VDP_Poke_16K(T_PATH, g_ScreenLayoutLow + (u16)ty * 32 + ((tx + 1) & 31));
	VDP_Poke_16K(T_PATH, g_ScreenLayoutLow + (u16)(ty + 1) * 32 + (tx & 31));
	VDP_Poke_16K(T_PATH, g_ScreenLayoutLow + (u16)(ty + 1) * 32 + ((tx + 1) & 31));
}

void StartFright()
{
	g_FrightTimer = FRIGHT_TIME;
	for (u8 i = 0; i < GHOST_COUNT; ++i)
		if (g_Ghosts[i].state == GHOST_NORMAL)
			g_Ghosts[i].state = GHOST_FRIGHT;
}

void UpdateFright()
{
	if (g_FrightTimer == 0)
		return;
	g_FrightTimer--;
	if (g_FrightTimer == 0)
	{
		for (u8 i = 0; i < GHOST_COUNT; ++i)
			if (g_Ghosts[i].state == GHOST_FRIGHT)
				g_Ghosts[i].state = GHOST_NORMAL;
	}
}

void EatDot(u8 cx, u8 cy)
{
	g_Dots[cy * MAZE_COLS + cx] = 0;
	g_DotsLeft--;
	g_Score += SCORE_DOT;
	ClearCell(cx, cy);
}

void EatPower(u8 cx, u8 cy)
{
	g_PowerDots[cy * MAZE_COLS + cx] = 0;
	g_DotsLeft--;
	g_Score += SCORE_POWER;
	ClearCell(cx, cy);
	StartFright();
	Sound_Play(900, 40, 40);
}

void UpdatePac()
{
	if (((g_PacX % CELL) == 0) && ((g_PacY % CELL) == 0))
	{
		u8 cx = (u8)(g_PacX / CELL);
		u8 cy = (u8)(g_PacY / CELL);
		if (g_PowerDots[cy * MAZE_COLS + cx])
			EatPower(cx, cy);
		else if (g_Dots[cy * MAZE_COLS + cx])
			EatDot(cx, cy);
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

void DrawPac()
{
	g_PacAnim++;
	u8 shape = SH_CLOSED;
	if ((g_PacDir != DIR_NONE) && ((g_PacAnim >> 3) & 1))
		shape = ShapeForDir(g_PacDir);
	VDP_SetSpritePattern(SPR_PAC, shape);
	VDP_SetSpritePosition(SPR_PAC, (u8)(g_PacX - g_CameraX), (u8)g_PacY);
}

//=============================================================================
// FANTASMAS: actualizacion y dibujo
//=============================================================================

void UpdateGhostMode()
{
	if (g_ModeTimer > 0)
	{
		g_ModeTimer--;
		if ((g_ModeTimer == 0) && (g_ModePhase < MODE_PHASES - 1))
		{
			g_ModePhase++;
			g_Mode = g_ModeOf[g_ModePhase];
			g_ModeTimer = g_ModeDur[g_ModePhase];
		}
	}
}

void UpdateGhosts()
{
	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		Ghost* g = &g_Ghosts[i];
		if (((g->x % CELL) == 0) && ((g->y % CELL) == 0))
			GhostChooseDir(i);
		switch (g->dir)
		{
		case DIR_RIGHT: g->x += GHOST_SPEED; break;
		case DIR_LEFT:  g->x -= GHOST_SPEED; break;
		case DIR_UP:    g->y -= GHOST_SPEED; break;
		case DIR_DOWN:  g->y += GHOST_SPEED; break;
		}
	}
}

void DrawGhosts()
{
	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		Ghost* g = &g_Ghosts[i];
		g->anim++;
		i16 sx = (i16)g->x - (i16)g_CameraX;
		if ((sx < -8) || (sx > 255))
		{
			VDP_HideSprite(g->sprId);
			continue;
		}

		u8 shape;
		u8 color;
		if (g->state == GHOST_EATEN)
		{
			shape = SH_EYES;
			color = COLOR_WHITE;
		}
		else if (g->state == GHOST_FRIGHT)
		{
			shape = ((g->anim >> 3) & 1) ? (SH_GHOST + 4) : SH_GHOST;
			color = COLOR_LIGHT_BLUE;
			if ((g_FrightTimer < 120) && ((g_FrightTimer >> 3) & 1))
				color = COLOR_WHITE;
		}
		else
		{
			shape = ((g->anim >> 3) & 1) ? (SH_GHOST + 4) : SH_GHOST;
			color = g->color;
		}

		VDP_FillVRAM_16K(color, g_SpriteColorLow + (u16)g->sprId * 16, 16);
		VDP_SetSpritePattern(g->sprId, shape);
		VDP_SetSpritePosition(g->sprId, (u8)sx, (u8)g->y);
	}
}

//=============================================================================
// HUD (vidas + puntuacion, sprites superpuestos al marco superior)
//=============================================================================

void DrawLives()
{
	for (u8 i = 0; i < LIVES_MAX; ++i)
	{
		if (i < g_Lives)
			VDP_SetSpritePosition(SPR_LIFE + i, (u8)(4 + i * 20), 0);
		else
			VDP_HideSprite(SPR_LIFE + i);
	}
}

void DrawScore()
{
	u16 s = g_Score;
	for (u8 i = 0; i < SCORE_DIGITS; ++i)
	{
		u8 spr = SPR_SCORE + (SCORE_DIGITS - 1 - i);
		u8 d = (u8)(s % 10);
		s /= 10;
		VDP_SetSpritePattern(spr, SH_DIGIT_0 + d * 4);
		VDP_SetSpritePosition(spr, (u8)(192 + (spr - SPR_SCORE) * 16), 0);
	}
}

void ShowHud(bool show)
{
	if (show)
	{
		DrawLives();
		DrawScore();
	}
	else
	{
		for (u8 i = 0; i < LIVES_MAX; ++i)
			VDP_HideSprite(SPR_LIFE + i);
		for (u8 i = 0; i < SCORE_DIGITS; ++i)
			VDP_HideSprite(SPR_SCORE + i);
	}
}

// Parpadeo de los power-pellets (solo los que aun no se han comido)
void UpdatePowerBlink()
{
	if (++g_PowerBlink >= 16)
	{
		g_PowerBlink = 0;
		g_PowerVisible = !g_PowerVisible;
		for (u8 k = 0; k < 4; ++k)
		{
			u8 cx = g_PowerCell[k * 2];
			u8 cy = g_PowerCell[k * 2 + 1];
			if (!g_PowerDots[cy * MAZE_COLS + cx])
				continue;

			u8 tl = g_PowerVisible ? T_POW_TL : T_PATH;
			u8 tr = g_PowerVisible ? T_POW_TR : T_PATH;
			u8 bl = g_PowerVisible ? T_POW_BL : T_PATH;
			u8 br = g_PowerVisible ? T_POW_BR : T_PATH;
			u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
			g_TileMap[base]                 = tl;
			g_TileMap[base + 1]             = tr;
			g_TileMap[base + TILE_COLS]     = bl;
			g_TileMap[base + TILE_COLS + 1] = br;

			u8 tx = cx * 2;
			u8 ty = cy * 2;
			VDP_Poke_16K(tl, g_ScreenLayoutLow + (u16)ty * 32 + (tx & 31));
			VDP_Poke_16K(tr, g_ScreenLayoutLow + (u16)ty * 32 + ((tx + 1) & 31));
			VDP_Poke_16K(bl, g_ScreenLayoutLow + (u16)(ty + 1) * 32 + (tx & 31));
			VDP_Poke_16K(br, g_ScreenLayoutLow + (u16)(ty + 1) * 32 + ((tx + 1) & 31));
		}
	}
}

//=============================================================================
// COLISION
//=============================================================================

bool GhostHitsPac(Ghost* g)
{
	i16 dx = (i16)g->x - (i16)g_PacX;
	i16 dy = (i16)g->y - (i16)g_PacY;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	return (dx < CELL - 4) && (dy < CELL - 4);
}

void StartDeath()
{
	g_State = STATE_DYING;
	g_DeathTimer = 90;
	VDP_HideSprite(SPR_PAC);
	Sound_Play(380, 70, 40);
}

//=============================================================================
// PANTALLAS DE TEXTO (titulo / game over)
//=============================================================================

void DrawBorder(u8 x0, u8 y0, u8 x1, u8 y1)
{
	VDP_Poke_16K(T_TL, g_ScreenLayoutLow + y0 * 32 + x0);
	VDP_Poke_16K(T_TR, g_ScreenLayoutLow + y0 * 32 + x1);
	VDP_Poke_16K(T_BL, g_ScreenLayoutLow + y1 * 32 + x0);
	VDP_Poke_16K(T_BR, g_ScreenLayoutLow + y1 * 32 + x1);
	for (u8 x = x0 + 1; x < x1; ++x)
	{
		VDP_Poke_16K(T_T, g_ScreenLayoutLow + y0 * 32 + x);
		VDP_Poke_16K(T_B, g_ScreenLayoutLow + y1 * 32 + x);
	}
	for (u8 y = y0 + 1; y < y1; ++y)
	{
		VDP_Poke_16K(T_L, g_ScreenLayoutLow + y * 32 + x0);
		VDP_Poke_16K(T_R, g_ScreenLayoutLow + y * 32 + x1);
	}
}

void SetTextColor(u8 color)
{
	Print_SetColor(color, COLOR_BLACK);
}

// Vuelca un mapa de tiles de pantalla (titulo/game over) al name table
void DrawTitleMap(const u8* map)
{
	for (u16 i = 0; i < TILE_ROWS * SCREEN_TILE_W; ++i)
		VDP_Poke_16K((u8)(TITLE_OFFSET + map[i]), g_ScreenLayoutLow + i);
}

void ShowTitleSprites(u8 frame)
{
	VDP_SetSpritePosition(SPR_PAC, 84, 100);
	VDP_SetSpritePattern(SPR_PAC, ((frame >> 3) & 1) ? SH_OPEN_R : SH_CLOSED);
	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		u8 shape = ((frame >> 3) & 1) ? (SH_GHOST + 4) : SH_GHOST;
		VDP_SetSpritePattern(g_Ghosts[i].sprId, shape);
		VDP_SetSpritePosition(g_Ghosts[i].sprId, (u8)(104 + i * 20), 100);
	}
}

void ShowGameOverSprites(u8 frame)
{
	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		u8 shape = ((frame >> 3) & 1) ? (SH_GHOST + 4) : SH_GHOST;
		VDP_SetSpritePattern(g_Ghosts[i].sprId, shape);
		VDP_SetSpritePosition(g_Ghosts[i].sprId, (u8)(88 + i * 28), 150);
	}
	VDP_HideSprite(SPR_PAC);
}

void DrawTitleScreen()
{
	DrawTitleMap(g_TitleMap);
	DrawBorder(2, 1, 29, 22);

	Print_SetTextFont(g_Font_MGL_Std0, FONT_OFFSET);
	SetTextColor(COLOR_LIGHT_YELLOW);

	Print_SetPosition(6, 9);
	Print_DrawText("COMECOCOS PARA MSX2+");

	Print_SetPosition(9, 16);
	Print_DrawText("PULSA ESPACIO");
	Print_SetPosition(11, 17);
	Print_DrawText("O FUEGO");

	Print_SetPosition(8, 21);
	Print_DrawText("PAPIPAPITO 2026");
}

void DrawGameOverScreen()
{
	DrawTitleMap(g_GameOverMap);
	DrawBorder(2, 3, 29, 20);

	Print_SetTextFont(g_Font_MGL_Std0, FONT_OFFSET);
	SetTextColor(COLOR_LIGHT_YELLOW);

	Print_SetPosition(9, 17);
	Print_DrawText("PULSA ESPACIO");
}

void RedrawBlink(bool show)
{
	if (show)
	{
		SetTextColor(COLOR_LIGHT_YELLOW);
		if (g_State == STATE_TITLE)
		{
			Print_SetPosition(9, 16);
			Print_DrawText("PULSA ESPACIO");
		}
		else
		{
			Print_SetPosition(9, 17);
			Print_DrawText("PULSA ESPACIO");
		}
	}
	else
	{
		u8 row = (g_State == STATE_TITLE) ? 16 : 17;
		VDP_FillVRAM_16K(0x00, g_ScreenLayoutLow + row * 32 + 8, 16);
	}
}

//=============================================================================
// LOGICA DE NIVEL / RESPAWN
//=============================================================================

void ResetPositions()
{
	g_PacX = CELL;
	g_PacY = CELL;
	g_PacDir = DIR_NONE;
	g_PacWantDir = DIR_NONE;
	g_PacAnim = 0;

	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		g_Ghosts[i].x = g_GhostStartX[i];
		g_Ghosts[i].y = g_GhostStartY[i];
		g_Ghosts[i].dir = DIR_LEFT;
		g_Ghosts[i].anim = 0;
		g_Ghosts[i].state = GHOST_NORMAL;
	}

	g_ModePhase = 0;
	g_Mode = MODE_ROAM;
	g_ModeTimer = g_ModeDur[0];
	g_FrightTimer = 0;

	InitScroll();
	UpdateCamera();

	VDP_SetSpritePattern(SPR_PAC, SH_CLOSED);
	VDP_SetSpritePosition(SPR_PAC, (u8)(g_PacX - g_CameraX), (u8)g_PacY);
	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		VDP_SetSpritePattern(g_Ghosts[i].sprId, SH_GHOST);
		VDP_SetSpritePosition(g_Ghosts[i].sprId, (u8)(g_Ghosts[i].x - g_CameraX), (u8)g_Ghosts[i].y);
		VDP_FillVRAM_16K(g_Ghosts[i].color, g_SpriteColorLow + (u16)g_Ghosts[i].sprId * 16, 16);
	}
}

void InitLevel()
{
	g_Lives = LIVES_MAX;
	g_Score = 0;
	g_Level = 1;
	VDP_LoadPattern_GM2(g_TilePattern, T_COUNT, 0);
	VDP_LoadColor_GM2(g_TileColor, T_COUNT, 0);
	GenMaze();
	BuildTileMap();
	ResetPositions();
	g_State = STATE_PLAYING;
}

void NextLevel()
{
	g_Level++;
	Sound_Play(500, 60, -30);
	VDP_LoadPattern_GM2(g_TilePattern, T_COUNT, 0);
	VDP_LoadColor_GM2(g_TileColor, T_COUNT, 0);
	GenMaze();
	BuildTileMap();
	ResetPositions();
}

//=============================================================================
// MAIN
//=============================================================================

void main()
{
	BIOS_SetKeyClick(FALSE);

	VDP_SetMode(VDP_MODE_GRAPHIC3);
	VDP_SetLayoutTable(0x3800);
	VDP_SetColorTable(0x2000);
	VDP_SetPatternTable(0x0000);
	VDP_SetSpritePatternTable(0x1800);
	VDP_SetSpriteAttributeTable(0x3E00);

	VDP_SetColor(0x11);
	VDP_ClearVRAM();
	VDP_EnableVBlank(TRUE);
	VDP_SetHorizontalMode(VDP_HSCROLL_SINGLE);

	SetPalette();

	VDP_LoadPattern_GM2(g_TilePattern, T_COUNT, 0);
	VDP_LoadColor_GM2(g_TileColor, T_COUNT, 0);
	VDP_LoadPattern_GM2(g_TitleTiles, sizeof(g_TitleTiles) / 8, TITLE_OFFSET);
	VDP_LoadColor_GM2(g_TitleColors, sizeof(g_TitleColors) / 8, TITLE_OFFSET);

	VDP_EnableSprite(TRUE);
	VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_1);
	VDP_LoadSpritePattern(g_PacPattern, 0, 5 * 4);
	VDP_LoadSpritePattern(g_GhostPattern, SH_GHOST, 2 * 4);
	VDP_LoadSpritePattern(g_EyesPattern, SH_EYES, 4);
	VDP_LoadSpritePattern(g_DigitPattern, SH_DIGIT_0, 10 * 4);

	// Sprites: 0=pac, 1..4=fantasmas, 5..7=vidas, 8..11=puntuacion
	VDP_SetSpriteExUniColor(SPR_PAC, 0, 0, SH_CLOSED, COLOR_LIGHT_YELLOW);
	for (u8 i = 0; i < GHOST_COUNT; ++i)
	{
		g_Ghosts[i].sprId = SPR_GHOST + i;
		g_Ghosts[i].color = g_GhostColor[i];
		g_Ghosts[i].state = GHOST_NORMAL;
		VDP_SetSpriteExUniColor(SPR_GHOST + i, 0, 0, SH_GHOST, g_GhostColor[i]);
	}
	for (u8 i = 0; i < LIVES_MAX; ++i)
		VDP_SetSpriteExUniColor(SPR_LIFE + i, (u8)(4 + i * 20), 0, SH_CLOSED, COLOR_LIGHT_YELLOW);
	for (u8 i = 0; i < SCORE_DIGITS; ++i)
		VDP_SetSpriteExUniColor(SPR_SCORE + i, (u8)(192 + i * 16), 0, SH_DIGIT_0, COLOR_WHITE);
	VDP_DisableSpritesFrom(SPR_SCORE + SCORE_DIGITS);

	Sound_Init();

	g_State = STATE_TITLE;
	g_StateTimer = 30;
	g_BlinkTimer = 0;
	g_BlinkOn = TRUE;
	g_DeathTimer = 0;
	DrawTitleScreen();
	ShowHud(FALSE);

	u8 frame = 0;

	while (!Keyboard_IsKeyPressed(KEY_ESC))
	{
		Halt();
		frame++;

		if (g_StateTimer)
			g_StateTimer--;

		switch (g_State)
		{
		case STATE_TITLE:
			ShowTitleSprites(frame);
			if (++g_BlinkTimer >= 30)
			{
				g_BlinkTimer = 0;
				g_BlinkOn = !g_BlinkOn;
				RedrawBlink(g_BlinkOn);
			}
			if ((g_StateTimer == 0) && StartPressed())
			{
				Sound_Play(600, 24, -12);
				InitLevel();
			}
			Sound_Update();
			break;

		case STATE_PLAYING:
			UpdateScroll();
			ReadInput();
			UpdatePac();
			UpdateGhostMode();
			UpdateFright();
			UpdateGhosts();
			UpdateCamera();
			for (u8 i = 0; i < GHOST_COUNT; ++i)
			{
				if (!GhostHitsPac(&g_Ghosts[i]))
					continue;
				if (g_Ghosts[i].state == GHOST_FRIGHT)
				{
					g_Ghosts[i].state = GHOST_EATEN;
					g_Score += SCORE_GHOST;
					Sound_Play(300, 24, 30);
				}
				else if (g_Ghosts[i].state == GHOST_NORMAL)
				{
					StartDeath();
					break;
				}
			}
			if ((g_State == STATE_PLAYING) && (g_DotsLeft == 0))
				NextLevel();
			DrawPac();
			DrawGhosts();
			DrawLives();
			DrawScore();
			UpdatePowerBlink();
			Sound_Update();
			break;

		case STATE_DYING:
			DrawGhosts();
			DrawLives();
			DrawScore();
			if (g_DeathTimer)
				g_DeathTimer--;
			if (g_DeathTimer == 0)
			{
				g_Lives--;
				if (g_Lives > 0)
				{
					ResetPositions();
					g_State = STATE_PLAYING;
				}
				else
				{
					g_State = STATE_GAMEOVER;
					g_StateTimer = 30;
					g_BlinkTimer = 0;
					g_BlinkOn = TRUE;
					DrawGameOverScreen();
					ShowHud(FALSE);
					Sound_Play(500, 80, 40);
				}
			}
			Sound_Update();
			break;

		case STATE_GAMEOVER:
			ShowGameOverSprites(frame);
			if (++g_BlinkTimer >= 30)
			{
				g_BlinkTimer = 0;
				g_BlinkOn = !g_BlinkOn;
				RedrawBlink(g_BlinkOn);
			}
			if ((g_StateTimer == 0) && StartPressed())
			{
				Sound_Play(600, 24, -12);
				InitLevel();
			}
			Sound_Update();
			break;
		}
	}

	BIOS_Exit(0);
}
