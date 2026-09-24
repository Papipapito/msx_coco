# MSX COCO

Juego tipo *comecocos* (Pac-Man) para **MSX2+**, escrito en C con la librería
[MSXgl](https://github.com/aoineko-fr/MSXgl) y compilado con SDCC. Aprovecha el
**scroll horizontal por hardware del chip de vídeo V9958** (registros R#26/R#27),
por lo que **requiere MSX2+** — no funciona en un MSX2 (V9938).

> **Versión: 0.4.0** — juego completo. Laberinto procedural, fichas y power-pellets,
> HUD con vidas y puntuación, pantallas gráficas de título/game over, 4 fantasmas
> con IA, colisión, game over y sonido PSG.

## Características

- **SCREEN 4 (Graphic 3)**: laberinto de tiles a pantalla completa + sprites 16×16 a color.
- **Scroll horizontal por hardware (V9958)**: el laberinto mide 512 px (el doble que
  la pantalla) y se desplaza con los registros R#26/R#27 del V9958. La *name table*
  de 32 columnas se usa de forma circular: solo se vuelca la columna que entra, sin
  repintar la pantalla → scroll suave por píxel.
- **Laberinto procedural**: generado por *recursive backtracker* sobre una rejilla
  de salas con lazos (rutas de escape), **distinto en cada partida** y en cada nivel.
- **Fichas y power-pellets**: fichas redondas repartidas por los pasillos (+10 pts)
  y power-pellets grandes cian que **asustan a los fantasmas** y permiten comerlos.
- **HUD** con la puntuación (dígitos) y las vidas (iconos del comecocos) en pantalla.
- **Muros con borde**: cada muro dibuja un contorno azul oscuro sobre relleno azul
  claro (detección de bordes por vecindad), con paleta MSX2 personalizada.
- **Comecocos** con movimiento en rejilla estilo Pac-Man (giros en intersecciones,
  colisión con las paredes) y animación de boca según la dirección.
- **4 fantasmas con IA**: personalidad propia (Blinky persigue, Pinky apunta por
  delante, Inky refleja, Clyde se dispersa de cerca) y ciclos de *dispersión* /
  *persecución*. Al comer una power-pellet quedan **asustados** (azules) y el
  comecocos puede comérselos; vuelven a su sala como ojos.
- **Colisión comecocos ↔ fantasmas**: al ser alcanzado se pierde una vida y se
  reinicia la ronda; con 0 vidas, *game over*.
- **Pantalla de inicio** y **game over** con logo grande "MSX COCO" / "GAME OVER"
  (arte de tiles), marco, comecocos y fantasmas animados, y "PULSA ESPACIO"
  parpadeante.
- **Sonido PSG**: barridos de tono para inicio, muerte, nivel, power-pellet y game over.

## Requisitos

- Un **MSX2+** real o un emulador que emule el **V9958** (openMSX, blueMSX, etc.).
- Para compilar: **WSL** (Ubuntu) con **SDCC** y la librería **MSXgl**.

## Controles

| Acción         | Tecla / Mando                              |
|----------------|--------------------------------------------|
| Mover          | Cursores del teclado **o** joystick puerto 1 |
| Empezar / salir | `SPACE` o botón de fuego (`ESC` para salir) |

## Cómo ejecutar

Carga `msx_coco.rom` como cartucho en tu emulador o flashea a un cartucho real.
Ejemplo con openMSX:

```powershell
& 'C:\Program Files\openMSX\openmsx.exe' -machine 'C-BIOS_MSX2+' -cart 'msx_coco.rom'
```

> Nota: para probar el scroll por hardware del V9958 conviene usar una máquina
> MSX2+ real, p. ej. `Panasonic_FS-A1WSX`.

## Cómo compilar

El binario se compila vía WSL + MSXgl. MSXgl debe estar clonado en `~/MSXgl`
dentro de WSL (`git clone --depth 1 https://github.com/aoineko-fr/MSXgl.git ~/MSXgl`).

```bash
wsl -d Ubuntu-24.04 bash -c "cd /mnt/c/Users/alber/msx_coco && MSXGL=\$HOME/MSXgl bash build.sh"
```

`build.sh` copia los fuentes al árbol de MSXgl, compila y devuelve `msx_coco.rom`
(32 KB, `ROM_32K`) a la carpeta del proyecto.

## Publicar una versión (CI)

El repositorio incluye un workflow de GitHub Actions
([.github/workflows/release.yml](.github/workflows/release.yml)) que **compila la
ROM en la nube y publica un release automáticamente** al subir un tag de versión:

```bash
git tag v0.1.0
git push origin v0.1.0
```

El workflow clona MSXgl, compila `msx_coco.rom` y crea el release con la ROM
adjunta (marcado como *pre-release* si el tag contiene `beta`/`alpha`/`rc`).
También puede lanzarse a mano desde la pestaña *Actions*.

## Estructura del proyecto

```
msx_coco/
├── msx_coco.c          # Todo el juego (fuente C)
├── msx_coco_data.h     # Datos estáticos (tiles, sprites, dígitos)
├── msxgl_config.h      # Configuración de módulos de MSXgl
├── project_config.js   # Configuración de compilación (ROM_32K, Machine 2P)
├── build.sh            # Script de compilación (WSL + MSXgl)
├── msx_coco.rom        # ROM compilada (32 KB)
├── tools/
│   ├── genpac.py       # Genera los patrones de sprite (comecocos + fantasma)
│   ├── gendigits.py    # Genera los dígitos 16×16 del marcador
│   ├── genpellets.py   # Genera fichas, power-pellets y ojos del fantasma
│   ├── gentitle.py     # Genera el arte (letras grandes) de título/game over
│   └── analyze.py      # Inspecciona sprites desde capturas PNG (depuración)
└── docs/
    ├── ARQUITECTURA.md # Detalles técnicos (scroll V9958, tiles, sprites…)
    └── CHANGELOG.md    # Historial de versiones
```

Más detalles técnicos en [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md).

## Próximos pasos

Posibles mejoras futuras: niveles con más velocidad, sonido de masticar al comer
puntos, marcador de récord (hi-score), y una "casa" para los fantasmas al centro.

## Créditos y licencia

- Programado por **Papipapito** con asistencia de Claude Code.
- Usa **MSXgl** de Guillaume "Aoineko" Blanchard (licencia CC BY-SA).
