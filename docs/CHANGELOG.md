# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/).

## [0.4.0] - 2026-09-24

### Añadido
- **Pantallas gráficas de título y game over**: logo grande "MSX COCO" y texto
  "GAME OVER" como arte de tiles (letras 5×7 a escala), con marco decorativo,
  el comecocos y los 4 fantasmas animados (sprites) y el "PULSA ESPACIO"
  parpadeante.
- Herramienta `tools/gentitle.py` que genera las letras grandes y el mapa de tiles.

### Corregido
- Los tiles del laberinto (fichas/power-pellets) ya no se pisan con la fuente al
  volver del título (la fuente se carga ahora en patrones 32+ y los tiles se
  recargan al empezar cada nivel).

## [0.3.0] - 2026-09-24

### Añadido
- **Fichas redondas**: los puntos ahora son círculos (fichas) en lugar de
  cuadraditos, más parecidos a las píldoras clásicas.
- **Power-pellets** (fichas grandes cian, parpadeantes) en 3 esquinas y el centro:
  al comerlas, los fantasmas se vuelven **asustados** (azules, movimiento
  aleatorio) durante ~6 s y el comecocos puede **comérselos** (+200 puntos). El
  fantasma comido vuelve a su sala convertido en ojos.
- Sonido al comer power-pellet y fantasma.
- Herramienta `tools/genpellets.py` para generar fichas, power-pellets y ojos.

### Corregido
- Puntos comestibles redibujados como círculos (antes parecían símbolos raros).

## [0.2.0] - 2026-09-24

### Añadido
- **Laberinto procedural**: *recursive backtracker* sobre una rejilla de 15×5 salas,
  con lazos (pasillos extra) para crear rutas de escape. Se genera un laberinto
  distinto en cada partida y en cada nivel (semilla a partir del reloj del sistema).
- **Puntos comestibles y marcador**: cocos en todos los pasillos; al comerlos se
  suman 10 puntos. Al acabar todos los cocos, se pasa al siguiente nivel.
- **HUD** con la puntuación (4 dígitos 16×16 en sprites) y las vidas (iconos del
  comecocos) fijos en pantalla.
- Herramienta `tools/gendigits.py` para generar los dígitos del marcador.

### Corregido
- **Colisión de los fantasmas**: al cambiar de velocidad entre dispersión y
  persecución, el fantasma podía quedar desalineado de la rejilla y atravesar
  paredes en línea recta. Ahora los fantasmas usan velocidad constante.

## [0.1.0] - 2026-09-24

Juego completo (estado jugable de principio a fin).

### Añadido
- **Colisión comecocos ↔ fantasmas**: al ser alcanzado se pierde una vida, con
  reinicio de ronda (comecocos y fantasmas vuelven a su posición inicial).
- **Vidas**: 3 vidas por partida, mostradas como iconos del comecocos en pantalla.
  Al agotarlas, *game over*.
- **Pantalla de inicio** con título, subtítulo, sprites animados (comecocos + 4
  fantasmas) y texto "PULSA ESPACIO" parpadeante.
- **Pantalla de game over** con opción de reiniciar la partida.
- **4 fantasmas con IA**:
  - Blinky (rojo) persigue al comecocos.
  - Pinky (magenta) apunta 4 casillas por delante.
  - Inky (cian) refleja la posición de Blinky respecto al comecocos.
  - Clyde (naranja) persigue de lejos y se dispersa de cerca.
  - Ciclos de *dispersión* (esquinas) y *persecución* con temporizador.
  - Elección de dirección *greedy* hacia el objetivo (sin dar marcha atrás salvo
    en callejón sin salida).
- **Gráficos mejorados**:
  - Paleta MSX2 personalizada (azules del muro, amarillo, colores de fantasmas).
  - Muros con contorno: detección de bordes por vecindad y tiles de esquina/borde.
- **Sonido PSG**: barrido de tono en inicio, muerte y game over.

### Cambiado
- El enemigo único con movimiento aleatorio se sustituye por 4 fantasmas con IA.
- Se añade el módulo `psg` de MSXgl a la compilación.

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
