# MSX COCO

Juego tipo *comecocos* (Pac-Man) para **MSX2+**, escrito en C con la librería
[MSXgl](https://github.com/aoineko-fr/MSXgl) y compilado con SDCC. Aprovecha el
**scroll horizontal por hardware del chip de vídeo V9958** (registros R#26/R#27),
por lo que **requiere MSX2+** — no funciona en un MSX2 (V9938).

> **Versión: 0.0.1-beta** — base jugable. El comecocos se mueve libremente por un
> laberinto más ancho que la pantalla, que hace scroll horizontal suave siguiéndolo.
> Hay un enemigo que deambula al azar (todavía sin colisión con el jugador).

## Características

- **SCREEN 4 (Graphic 3)**: laberinto de tiles a pantalla completa + sprites 16×16 a color.
- **Scroll horizontal por hardware (V9958)**: el laberinto mide 512 px (el doble que
  la pantalla) y se desplaza con los registros R#26/R#27 del V9958. La *name table*
  de 32 columnas se usa de forma circular: solo se vuelca la columna que entra, sin
  repintar la pantalla → scroll suave por píxel.
- **Comecocos** con movimiento en rejilla estilo Pac-Man (giros en intersecciones,
  colisión con las paredes) y animación de boca según la dirección.
- **Enemigo (fantasma)** con movimiento aleatorio por el laberinto.
- **Laberinto** generado y validado por conectividad (todas las celdas alcanzables).

## Requisitos

- Un **MSX2+** real o un emulador que emule el **V9958** (openMSX, blueMSX, etc.).
- Para compilar: **WSL** (Ubuntu) con **SDCC 4.2.0** y la librería **MSXgl**.

## Controles

| Acción | Tecla / Mando |
|--------|---------------|
| Mover  | Cursores del teclado **o** joystick del puerto 1 |
| Salir  | `ESC` |

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
wsl -d Ubuntu-22.04 bash -c "cd /mnt/c/Users/alber/msx_coco && MSXGL=\$HOME/MSXgl bash build.sh"
```

`build.sh` copia los fuentes al árbol de MSXgl, compila y devuelve `msx_coco.rom`
(32 KB, `ROM_32K`) a la carpeta del proyecto.

## Estructura del proyecto

```
msx_coco/
├── msx_coco.c          # Todo el juego (fuente C)
├── msxgl_config.h      # Configuración de módulos de MSXgl
├── project_config.js   # Configuración de compilación (ROM_32K, Machine 2P)
├── build.sh            # Script de compilación (WSL + MSXgl)
├── msx_coco.rom        # ROM compilada (32 KB)
├── tools/
│   ├── genpac.py       # Genera los patrones de sprite (comecocos + fantasma)
│   ├── genmap.py       # Genera y valida el laberinto (BFS de conectividad)
│   └── analyze.py      # Inspecciona sprites desde capturas PNG (depuración)
└── docs/
    ├── ARQUITECTURA.md # Detalles técnicos (scroll V9958, tiles, sprites…)
    └── CHANGELOG.md    # Historial de versiones
```

Más detalles técnicos en [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md).

## Estado y próximos pasos

Esto es una **beta** centrada en el motor (laberinto + scroll + movimiento). Pendiente:
colisión comecocos↔enemigo, cocos comestibles y marcador, más enemigos con IA,
sonido, y un trazado de laberinto más elaborado.

## Créditos y licencia

- Programado por **Papipapito** con asistencia de Claude Code.
- Usa **MSXgl** de Guillaume "Aoineko" Blanchard (licencia CC BY-SA).
