# MSX COCO

Juego tipo *comecocos* (Pac-Man) para **MSX2+**, escrito en C con la librería
[MSXgl](https://github.com/aoineko-fr/MSXgl) y compilado con SDCC. Aprovecha el
**scroll horizontal por hardware del chip de vídeo V9958** (registros R#26/R#27),
por lo que **requiere MSX2+** — no funciona en un MSX2 (V9938).

> **Versión: 0.2.0** — juego completo: 3 fantasmas con personalidades, 3 vidas,
> power pellets, pantalla de título, niveles con temas de color y sonido FM/PSG.

## Características

- **SCREEN 4 (Graphic 3)**: laberinto de tiles a pantalla completa + sprites 16×16 a color.
- **Scroll horizontal por hardware (V9958)**: el laberinto mide 512 px (el doble que
  la pantalla) y se desplaza con los registros R#26/R#27. La *name table* de 32
  columnas se usa de forma circular: solo se vuelca la columna que entra, sin
  repintar la pantalla → scroll suave por píxel.
- **Estética neón**: paredes auto-tiled (20 variantes elegidas por vecindad) con
  contorno brillante sobre relleno oscuro con scanlines, y **paleta programable
  del MSX2** con un tema por nivel: cian → verde matrix → magenta synthwave → ámbar.
- **3 fantasmas con personalidades**: rojo *perseguidor* (va a por ti), rosa
  *emboscador* (apunta por delante de tu dirección) y cian *errático* — que además
  lleva **gafas de sol**. Liberación escalonada y velocidad creciente por nivel.
- **Power pellets**: 4 por nivel (parpadean por ciclo de paleta, coste cero); al
  comerlos los fantasmas huyen en azul y puedes comértelos en cadena
  (20/40/80/160 puntos). Aviso de fin de efecto con parpadeo blanco.
- **3 vidas** con colisión, animación de muerte en 5 fases, banner READY!,
  GAME OVER y hi-score de la sesión.
- **Puntos-gema** dorados con sombreado de dos tonos.
- **Marcador y vidas como sprites fijos** (el HUD no se ve afectado por el scroll).
- **Niveles infinitos**: laberinto aleatorio conexo por construcción en cada nivel,
  con semilla distinta en cada partida.
- **Sonido**: 6 efectos en **FM (YM2413 / MSX-Music)** con *fallback* automático a
  PSG si no hay chip FM — comer, pellet, comer fantasma, muerte, nivel superado
  y jingle de inicio.
- **Pantalla de título** con logo a doble tamaño, desfile de personajes y attract.

## Requisitos

- Un **MSX2+** real o un emulador que emule el **V9958** (openMSX, blueMSX, etc.).
- Para compilar: **WSL** (Ubuntu) con **SDCC 4.2.0** y la librería **MSXgl**.

## Controles

| Acción   | Tecla / Mando |
|----------|---------------|
| Empezar  | `ESPACIO` |
| Mover    | Cursores del teclado **o** joystick del puerto 1 |

## Cómo ejecutar

Carga `msx_coco.rom` como cartucho en tu emulador o flashea a un cartucho real.
Ejemplo con openMSX:

```powershell
& 'C:\Program Files\openMSX\openmsx.exe' -machine 'C-BIOS_MSX2+' -cart 'msx_coco.rom'
```

## Cómo compilar

El binario se compila vía WSL + MSXgl. MSXgl debe estar clonado en `~/MSXgl`
dentro de WSL (`git clone --depth 1 https://github.com/aoineko-fr/MSXgl.git ~/MSXgl`).

```bash
wsl -d Ubuntu-22.04 bash -c "cd /mnt/c/ruta/al/repo/msx_coco && MSXGL=\$HOME/MSXgl bash build.sh"
```

`build.sh` copia los fuentes al árbol de MSXgl, compila y devuelve `msx_coco.rom`
(32 KB, `ROM_32K`) a la carpeta del proyecto.

## Publicar una versión (CI)

El repositorio incluye un workflow de GitHub Actions
([.github/workflows/release.yml](.github/workflows/release.yml)) que **compila la
ROM en la nube y publica un release automáticamente** al subir un tag de versión:

```bash
git tag v0.2.0
git push origin v0.2.0
```

El workflow clona MSXgl, compila `msx_coco.rom` y crea el release con la ROM
adjunta (marcado como *pre-release* si el tag contiene `beta`/`alpha`/`rc`).
También puede lanzarse a mano desde la pestaña *Actions* o con
`gh workflow run "Release ROM" -f tag=v0.2.0`.

## Estructura del proyecto

```
msx_coco/
├── msx_coco.c          # Todo el juego (fuente C)
├── msxgl_config.h      # Configuración de módulos de MSXgl
├── project_config.js   # Configuración de compilación (ROM_32K, Machine 2P)
├── build.sh            # Script de compilación (WSL + MSXgl)
├── msx_coco.rom        # ROM compilada (32 KB)
├── tools/
│   ├── genpac.py       # Genera los sprites (pac, fantasmas, gafas, muerte, letras)
│   ├── genwalls.py     # Genera los tiles (paredes auto-tiled, gemas, pellets)
│   ├── gendigits.py    # Genera los dígitos del marcador
│   ├── genmap.py       # Genera y valida el laberinto (BFS de conectividad)
│   └── analyze.py      # Inspecciona sprites desde capturas PNG (depuración)
└── docs/
    ├── ARQUITECTURA.md # Detalles técnicos (scroll V9958, tiles, sprites…)
    └── CHANGELOG.md    # Historial de versiones
```

Más detalles técnicos en [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md).

## Estado y próximos pasos

La **0.2.0** es la primera versión con el ciclo de juego completo (vidas, niveles,
game over). Ideas para el futuro: música de fondo, tabla de tiempos para PAL
(los timers están calibrados a 60 Hz), fruta de bonus y el modo 2 jugadores por
WiFi UNAPI (en desarrollo aparte).

## Créditos y licencia

- Programado por **Papipapito** con asistencia de Claude Code.
- Usa **MSXgl** de Guillaume "Aoineko" Blanchard (licencia CC BY-SA).
