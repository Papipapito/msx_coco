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

#define PAC_SPEED     2
#define ENEMY_SPEED   2
#define CAM_CENTER    ((SCREEN_W / 2) - (CELL / 2)) // 120

//=============================================================================
// TILES
//=============================================================================

#define T_PATH        0
#define T_WALL        1

const u8 g_TilePattern[2 * 8] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0: pasillo
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 1: pared
};

const u8 g_TileColor[2 * 8] =
{
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, // 0: negro
	0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, // 1: azul
};

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

const u8 g_PacPattern[5 * 4 * 8] =
{
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // CLOSED
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7F, 0xFC, 0xF8, 0xF8, 0xFC, 0x7F, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0xE0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xE0, 0xF8, 0xF0, 0xC0, // OPEN_R
	0x03, 0x0F, 0x1F, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x1F, 0x0F, 0x03, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFE, 0x3F, 0x1F, 0x1F, 0x3F, 0xFE, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // OPEN_L
	0x00, 0x00, 0x00, 0x20, 0x60, 0x70, 0xF0, 0xF8, 0xFC, 0xFC, 0x7E, 0x7F, 0x3F, 0x1F, 0x0F, 0x03, 0x00, 0x00, 0x00, 0x04, 0x06, 0x0E, 0x0F, 0x1F, 0x3F, 0x3F, 0x7E, 0xFE, 0xFC, 0xF8, 0xF0, 0xC0, // OPEN_U
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x7E, 0xFC, 0xFC, 0xF8, 0xF0, 0x70, 0x60, 0x20, 0x00, 0x00, 0x00, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0x7E, 0x3F, 0x3F, 0x1F, 0x0F, 0x0E, 0x06, 0x04, 0x00, 0x00, 0x00, // OPEN_D
};

const u8 g_GhostPattern[2 * 4 * 8] =
{
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x73, 0x73, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xCE, 0xCE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x39, 0x0C, 0x02, // GHOST_A
	0x03, 0x0F, 0x1F, 0x3F, 0x7F, 0x73, 0x73, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x9C, 0x30, 0x40, 0xC0, 0xF0, 0xF8, 0xFC, 0xFE, 0xCE, 0xCE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE7, 0xC3, 0x81, // GHOST_B
};

//=============================================================================
// MAPA — tools/genmap.py (1=pared, 0=camino), conectividad validada
//=============================================================================

const u8 g_MazeData[MAZE_ROWS * MAZE_COLS] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

//=============================================================================
// DATOS EN RAM
//=============================================================================

u8  g_TileMap[TILE_ROWS * TILE_COLS];

u16 g_PacX, g_PacY;
u8  g_PacDir, g_PacWantDir, g_PacAnim;

u16 g_EnX, g_EnY;
u8  g_EnDir, g_EnAnim;

u8  g_Sprt, g_EnSprt;

u16 g_CameraX;     // posicion de camara en el mundo (px), 0..MAX_SCROLL
u16 g_DrawnLeft;   // columna de tile del mundo en el borde izquierdo del name table

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

void BuildTileMap()
{
	for (u8 cy = 0; cy < MAZE_ROWS; ++cy)
	{
		for (u8 cx = 0; cx < MAZE_COLS; ++cx)
		{
			u8  t = IsWallCell(cx, cy) ? T_WALL : T_PATH;
			u16 base = ((u16)(cy * 2)) * TILE_COLS + (cx * 2);
			g_TileMap[base]                 = t;
			g_TileMap[base + 1]             = t;
			g_TileMap[base + TILE_COLS]     = t;
			g_TileMap[base + TILE_COLS + 1] = t;
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

// Aplica la camara: vuelca las columnas que entran y desplaza por hardware
void UpdateScroll()
{
	u16 newLeft = g_CameraX >> 3;
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

	// Scroll horizontal del V9958 en modo de pagina simple (R#26/R#27)
	VDP_SetHorizontalMode(VDP_HSCROLL_SINGLE);

	VDP_LoadPattern_GM2(g_TilePattern, 2, 0);
	VDP_LoadColor_GM2(g_TileColor, 2, 0);

	BuildTileMap();

	Math_SetRandomSeed8(0x37);

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

	VDP_EnableSprite(TRUE);
	VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_1);
	VDP_LoadSpritePattern(g_PacPattern, 0, 5 * 4);
	VDP_LoadSpritePattern(g_GhostPattern, SH_GHOST, 2 * 4);
	VDP_SetSpriteExUniColor(g_Sprt,   (u8)g_PacX, (u8)g_PacY, SH_CLOSED, COLOR_LIGHT_YELLOW);
	VDP_SetSpriteExUniColor(g_EnSprt, (u8)g_EnX,  (u8)g_EnY,  SH_GHOST,  COLOR_LIGHT_RED);
	VDP_DisableSpritesFrom(g_EnSprt + 1);

	InitScroll();

	while (!Keyboard_IsKeyPressed(KEY_ESC))
	{
		Halt();
		// Render en V-Blank: volcado de columna entrante + scroll por hardware
		UpdateScroll();
		DrawPac();
		DrawEnemy();
		// Logica para el frame siguiente
		ReadInput();
		UpdatePac();
		UpdateEnemy();
		UpdateCamera();
	}

	BIOS_Exit(0);
}
