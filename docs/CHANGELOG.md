# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/).

## [0.2.0] - 2026-08-05

Primera versión con el ciclo de juego completo.

### Añadido
- **3 fantasmas con personalidades**: rojo perseguidor (minimiza distancia al pac),
  rosa emboscador (apunta 4 celdas por delante) y cian errático **con gafas de sol**.
  Liberación escalonada (0/2/4 s), despertar por proximidad y velocidad por nivel.
- **Colisión y 3 vidas**: animación de muerte en 5 fases, banner READY!, GAME OVER
  (con alternancia score↔hi-score) y hi-score de la sesión.
- **Power pellets**: 4 por nivel, uno por cuadrante; modo *frightened* (~7 s,
  decreciente por nivel) con huida al 50%, parpadeo de aviso y cadena de puntos
  20/40/80/160. El pellet asusta también a los fantasmas aún aparcados.
- **Pantalla de título**: logo "MSX COCO" con sprites a doble escala, desfile de
  personajes, PUSH SPACE parpadeante, hi-score e indicador FM.
- **Estética neón**: paredes auto-tiled (16 variantes por máscara de vecindad + 4
  esquinas) con contorno brillante, relleno oscuro y scanlines; paleta programable
  con 4 temas por nivel (cian/verde matrix/magenta synthwave/ámbar); puntos-gema
  dorados con sombreado de dos tonos.
- **Sonido FM (YM2413/MSX-Music) con fallback PSG**: 6 efectos (comer, pellet,
  comer fantasma, muerte, nivel superado, inicio) con motor de prioridades.
- HUD de vidas (mini-pacs) y marcador como sprites fijos fuera del área de juego.
- Puntos comestibles y marcador; niveles infinitos con laberinto aleatorio conexo
  y semilla distinta por partida.
- Herramientas nuevas: `tools/genwalls.py` (tiles) y `tools/gendigits.py` (dígitos).

### Cambiado
- Animación del pac: wakka de 4 fases (cerrado→medio→abierto→medio) sincronizado
  a una celda por ciclo.
- La pantalla de debug del arranque se sustituye por la pantalla de título.
- `ESC` ya no sale del juego (es un cartucho; el bucle es infinito).

### Arreglado
- Scroll suave de verdad: `VDP_EnableMask` + orden offset-antes-de-columnas
  (elimina el salto de la columna izquierda cada 8 px).
- Corrupción de la IA por un aliasing de arrays locales de SDCC (dos arrays de
  `ChooseDir` compartían almacenamiento de pila): candidatos movidos a un array
  global y distancias en escalares, verificado sobre el ensamblador generado.
- Workaround del puerto de datos del MSX-Music (0x7D) que falta en el checkout
  actual de MSXgl.

## [0.0.1-beta] - 2026-05-28

Primera versión jugable (base del motor).

### Añadido
- Laberinto de tiles a pantalla completa en SCREEN 4 (Graphic 3), MSX2+.
- **Scroll horizontal por hardware del V9958** (R#26/R#27) con *name table* circular:
  el laberinto (512 px) hace scroll suave siguiendo al comecocos sin repintar la pantalla.
- Comecocos (sprite 16×16) con movimiento en rejilla, colisión con paredes y animación
  de boca según la dirección.
- Enemigo (fantasma 16×16) con movimiento aleatorio por el laberinto (sin colisión con
  el jugador todavía).
- Mapa generado y validado por conectividad (BFS).
- Control por teclado (cursores) y joystick (puerto 1).
- Herramientas de generación de assets (`tools/genpac.py`, `tools/genmap.py`) y de
  depuración (`tools/analyze.py`).

### Notas
- Requiere **MSX2+** (los registros de scroll R#26/R#27 no existen en el V9938/MSX2).

### Pendiente
- Colisión comecocos ↔ enemigo y lógica de juego (vidas, game over).
- Cocos comestibles y marcador de puntos.
- Más enemigos con IA de persecución.
- Sonido (PSG).
- Trazado de laberinto más elaborado.
