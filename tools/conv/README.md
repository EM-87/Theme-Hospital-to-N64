# Conversor de datos (fase 5, hito 0)

Lee la copia de Theme Hospital de cada usuario (`$TH_DATA_DIR`) y genera los assets del motor en `motor/assets/`, sin comprimir. Al compilar la ROM, `motor/Makefile` los comprime en `motor/filesystem/`. Nada de lo que genera va al repositorio.

```bash
source tools/env.sh
python3 -I tools/conv/conv.py                    # todos los pasos
python3 -I tools/conv/conv.py graficos           # solo un paso (graficos, animaciones)
python3 -I tools/conv/conv.py --vistas DIR       # además, vistas previas en PNG
```

| Módulo | Qué hace |
|---|---|
| `rnc.py` | Descompresor RNC (port del de CorsixTH, MIT). Da lo mismo que `rnc_decode` en los 241 ficheros RNC de los datos (16,3 MB) |
| `paleta.py` | `MPALETTE.DAT` a RGBA5551, con las paletas fantasma gris y roja de CorsixTH |
| `hojas.py` | Hojas `.TAB`/`.DAT` al formato TSH1: CI8, filas de 8 B, misma numeración. Usa el decodificador de `tools/datos/graficos.py` |
| `anim.py` | Tablas de animación de `DATAM` (START, FRA, LIST, ELE) al formato TAN1, con la duración de cada animación calculada como en CorsixTH |
| `conv.py` | La orden principal y el formato TPL1 de las paletas |

Formatos (big-endian, como la N64):
- **TSH1** (hoja de sprites): descrito en `hojas.py`.
- **TPL1** (paletas): `'TPL1'`, u16 número de paletas, u16 0, y 512 B por paleta. Las paletas son: normal, fantasma gris y fantasma rojo.

Comprobaciones al convertir:
- **Hojas:** cada hoja leída de vuelta tiene que dar los mismos píxeles.
- **Animaciones:**
  - el encadenamiento de fotogramas de `DATAM` tiene que ser el de `DATA`, que es el que usa CorsixTH;
  - la tabla escrita se lee de vuelta igual;
  - con `--oraculo-anim FICHERO` (el volcado de `tools/construccion/metodo/oraculo/sonda_anim.lua`), las duraciones se comparan con las de CorsixTH. Dan 5.183 iguales de 5.186, y las 3 distintas son los ajustes del Lua de CorsixTH (2348, 2938 y 4086).
