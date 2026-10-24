# Conversor de datos (fase 5, hito 0)

Lee la copia de Theme Hospital de cada usuario (`$TH_DATA_DIR`) y genera los assets del motor en `motor/assets/`, sin comprimir. Al compilar la ROM, `motor/Makefile` los comprime en `motor/filesystem/`. Nada de lo que genera va al repositorio. El paso de reglas necesita además `lua5.4` y el código de CorsixTH v0.70.1 (`$CORSIXTH_DIR`, que deja `tools/setup.sh`).

```bash
source tools/env.sh
python3 -I tools/conv/conv.py                    # todos los pasos
python3 -I tools/conv/conv.py graficos           # solo un paso (graficos, animaciones, niveles, reglas)
python3 -I tools/conv/conv.py --vistas DIR       # además, vistas previas en PNG
```

| Módulo | Qué hace |
|---|---|
| `rnc.py` | Descompresor RNC (port del de CorsixTH, MIT). Da lo mismo que `rnc_decode` en los 241 ficheros RNC de los datos (16,3 MB) |
| `paleta.py` | `MPALETTE.DAT` a RGBA5551, con las paletas fantasma gris y roja de CorsixTH |
| `hojas.py` | Hojas `.TAB`/`.DAT` al formato TSH1: CI8, filas de 8 B, misma numeración. Usa el decodificador de `tools/datos/graficos.py` |
| `anim.py` | Tablas de animación de `DATAM` (START, FRA, LIST, ELE) al formato TAN1, con la duración de cada animación calculada como en CorsixTH |
| `niveles.py` | Mapas de los 12 niveles (`LEVELS/LEVEL.Lx`) al formato TMP1: lee como `load_from_th_file` de CorsixTH, con su tabla de bloques |
| `reglas.lua` | Extrae las reglas del código de CorsixTH con `lua5.4` y stubs: objetos, salas, enfermedades (con el aspecto de los pacientes como distribución exacta), diagnósticos, marcadores, duraciones fijadas, animaciones de caminar, configuración por defecto, `base_config.lua` y los 36 niveles de la campaña. Escribe el JSON canónico de `json_canonico.lua` |
| `reglas.py` | Ese JSON al formato TRG1, con la ida y vuelta comprobada. `python3 -I tools/conv/reglas.py comprobar <th64sim> motor/assets` compara el lector del núcleo con el del conversor |
| `comparar_reglas.py` | Compara las reglas del extractor con las que tiene CorsixTH en memoria (`bench/scenarios/volcar_reglas.lua`) |
| `conv.py` | La orden principal y el formato TPL1 de las paletas |

Formatos (big-endian, como la N64):
- **TSH1** (hoja de sprites): descrito en `hojas.py`.
- **TPL1** (paletas): `'TPL1'`, u16 número de paletas, u16 0, y 512 B por paleta. Las paletas son: normal, fantasma gris y fantasma rojo.
- **TRG1** (reglas): un árbol de valores como el de Lua, descrito en `reglas.py`. Hay un fichero con las definiciones (`reglas/definiciones.trg`) y uno por nivel y dificultad (`reglas/full01.trg`…).
- **TMP1** (mapa de un nivel): descrito en `niveles.py`. Guarda lo que trae el fichero y nada más: 10 B por casilla, con las paredes actuales y las originales (con las divisorias entre parcelas). Lo que CorsixTH hace después al cargar (parcelas sin dueño, correcciones de `map.lua`) es lógica del juego y está en el núcleo (`motor/core/src/map.c`).

Comprobaciones al convertir:
- **Hojas:** cada hoja leída de vuelta tiene que dar los mismos píxeles.
- **Animaciones:**
  - el encadenamiento de fotogramas de `DATAM` tiene que ser el de `DATA`, que es el que usa CorsixTH;
  - la tabla escrita se lee de vuelta igual;
  - con `--oraculo-anim FICHERO` (el volcado de `tools/construccion/metodo/oraculo/sonda_anim.lua`), las duraciones se comparan con las de CorsixTH. Dan 5.183 iguales de 5.186, y las 3 distintas son los ajustes del Lua de CorsixTH (2348, 2938 y 4086).
- **Reglas:**
  - cada fichero TRG1 se lee de vuelta igual, con igualdad estricta (un entero no es un real);
  - contra CorsixTH, fuera del conversor:

    ```bash
    tools/bench_run.sh bench/scenarios/volcar_reglas.lua $TH64_WORK/resultados/hito0/oraculo_reglas
    python3 -I tools/conv/conv.py --vistas $TH64_WORK/resultados/hito0/vistas reglas
    python3 -I tools/conv/comparar_reglas.py $TH64_WORK/resultados/hito0/vistas/reglas.json $TH64_WORK/resultados/hito0/oraculo_reglas
    ```

    Da 60 de 60 objetos, 23 de 23 salas, 34 de 34 enfermedades, 9 de 9 diagnósticos y 36 de 36 niveles iguales.
- **Niveles:** el conversor no los compara por sí mismo. Lo hace el núcleo, que es el que los interpreta: `th64sim mapa` carga cada nivel y lo compara con el volcado de CorsixTH ([`docs/05_prototipo.md`](../../docs/05_prototipo.md#hito-2--mapa-isométrico-con-scroll-a-30-fps)).
