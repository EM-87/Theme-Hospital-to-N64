# Oráculos del mapa (fase 5, hitos 2 y 3)

Comprueban el mapa del motor contra CorsixTH: lo que hay en cada casilla, los caminos del A*, cómo camina un humanoide y lo que se ve en pantalla. Antes de nada: `source tools/env.sh`, `python3 -I tools/conv/conv.py niveles` y el núcleo compilado en `motor/build/pc` (`cmake --build motor/build/pc`).

## Casillas

`bench/scenarios/volcar_mapa.lua` carga un nivel en CorsixTH y vuelca, de cada una de las 16.384 casillas, las capas (suelo y paredes) y las banderas de `getCellFlags`. `th64sim mapa` carga el mismo nivel en el núcleo y lo compara campo a campo:

```bash
tools/bench_run.sh bench/scenarios/volcar_mapa.lua $TH64_WORK/resultados/hito2/oraculo --th64-nivel=1
motor/build/pc/th64sim mapa motor/assets/niveles/nivel01.tmp 1 $TH64_WORK/resultados/hito2/oraculo/mapa_nivel1.tsv
```

Las diferencias a 3 casillas o menos de un objeto del mapa (puertas de entrada, objetos de las salas ya hechas) se cuentan aparte: son banderas que ponen los objetos al colocarse, y eso llega con los objetos.

## Caminos y caminatas

`caminos.sh` compara el A* y la acción de caminar del núcleo:
- **Caminos.** En cada nivel, `bench/scenarios/caminos.lua` pide a CorsixTH (`World:getPath`) 1.000 caminos entre pares de casillas transitables. Los pares salen de un generador congruencial con semilla fija. `th64sim caminos` los repite con el A* del núcleo y los compara casilla a casilla.
- **Caminatas.** `bench/scenarios/caminata.lua` pone un «Standard Male Patient» en una casilla y le manda caminar a otra. Después vuelca su estado tras cada tick del mundo: casilla, desplazamiento, animación, fotograma y banderas. `th64sim caminata` lo repite con el núcleo y lo compara tick a tick.

```bash
tools/oraculo_mapa/caminos.sh                 # los 12 niveles y las 16 caminatas de docs/05_prototipo.md
tools/oraculo_mapa/caminos.sh "1 5" mias.txt  # solo esos niveles y las caminatas de mias.txt («nivel x1,y1;x2,y2 ticks»)
```

Lo que se espera: todos los caminos iguales (también los pares sin camino) y todos los ticks iguales.

## Pantalla

`oraculo.sh` hace capturas del mapa en CorsixTH (`bench/scenarios/captura_nivel.lua`, sin ventanas ni cursor). Dibuja lo mismo con `th64sim vista` y los gráficos de alta resolución (`DATA/VBLK-0`, los que usa CorsixTH) y compara píxel a píxel:

```bash
tools/oraculo_mapa/oraculo.sh 1 "i;71,71;57,50;83,58;0,0"   # nivel, casillas en el centro («i» = cámara inicial)
```

`escenas.sh` hace lo mismo con un humanoide que camina:
- CorsixTH (`caminata.lua` con `--th64-tick-png`) se para en un tick y hace una captura centrada en él;
- `th64sim escena` repite la caminata hasta ese tick y dibuja el mapa con el humanoide, con los sprites y las animaciones de alta resolución (`VSPR-0`, `VSTART`…`VELE`).

```bash
tools/oraculo_mapa/escenas.sh                 # las 8 escenas de docs/05_prototipo.md
```

| Fichero | Qué hace |
|---|---|
| `oraculo.sh <nivel> [casillas] [dir]` | Todo el proceso para un nivel. Deja en `dir` (por defecto `$TH64_WORK/resultados/hito2/vista`) las capturas, las vistas y las diferencias en rojo |
| `escenas.sh [lista] [dir]` | Lo mismo para escenas con un humanoide (por defecto en `$TH64_WORK/resultados/hito3/escenas`) |
| `caminos.sh [niveles] [lista] [dir]` | Los oráculos de caminos y caminatas (por defecto en `$TH64_WORK/resultados/hito3`) |
| `alta.py <dir>` | Los gráficos de alta resolución de `DATA` en los formatos del motor: `VBLK-0` y `VSPR-0` a TSH1, las tablas de animación a TAN1 y `MPALETTE.DAT` descomprimido. Para el panel inferior de la fase 6, también `PANEL02V`, `MPOINTER`, las fuentes `FONT05V`, `FONT16V`, `FONT01V`, `FONT124V` y `FONT00V` de `QDATA` y la paleta `PREF01V.PAL`. Los scripts lo llaman solos y lo dejan en `$TH64_WORK/resultados/alta` |
| `comparar.py <captura.png> <vista.ppm> [dif.png] [--mezcla m.pgm] [--entidades e.pgm] [--excluir x0,y0,x1,y1]` | Cuenta los píxeles iguales, los que difieren en 2 o menos (el redondeo de la mezcla de las sombras) y los distintos, con las zonas distintas agrupadas. Con `--entidades` cuenta aparte los píxeles del humanoide del motor |

Lo que se espera:
- **Mapa.** Ningún píxel distinto, salvo en las entidades que el motor aún no dibuja: puertas, personal y las sombras que proyectan las puertas. Los píxeles de sombra pueden diferir en 1 o 2.
- **Humanoide.** Diferencias de 5 o menos en su interior y de 7 o menos en su borde: CorsixTH dibuja las animaciones con filtrado lineal.

Resultados en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#hito-3--un-humanoide-que-camina-por-el-mapa).

La comparación entre lo que dibuja el RDP y el rasterizador de referencia no está aquí: la hace la ROM de pruebas en la N64 (`tools/motor_check.sh`).
