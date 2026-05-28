# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/).

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
