# Oráculos del mapa (fase 5, hito 2)

Comprueban el mapa del motor contra CorsixTH: lo que hay en cada casilla y lo que se ve en pantalla. Antes de nada: `source tools/env.sh`, `python3 -I tools/conv/conv.py niveles` y el núcleo compilado en `motor/build/pc` (`cmake --build motor/build/pc`).

## Casillas

`bench/scenarios/volcar_mapa.lua` carga un nivel en CorsixTH y vuelca, de cada una de las 16.384 casillas, las capas (suelo y paredes) y las banderas de `getCellFlags`. `th64sim mapa` carga el mismo nivel en el núcleo y lo compara campo a campo:

```bash
tools/bench_run.sh bench/scenarios/volcar_mapa.lua $TH64_WORK/resultados/hito2/oraculo --th64-nivel=1
motor/build/pc/th64sim mapa motor/assets/niveles/nivel01.tmp 1 $TH64_WORK/resultados/hito2/oraculo/mapa_nivel1.tsv
```

Las diferencias a 3 casillas o menos de un objeto del mapa (puertas de entrada, objetos de las salas ya hechas) se cuentan aparte: son banderas que ponen los objetos al colocarse, y eso llega con los objetos.

## Pantalla

`oraculo.sh` hace capturas del mapa en CorsixTH (`bench/scenarios/captura_nivel.lua`, sin ventanas ni cursor), dibuja lo mismo con `th64sim vista` y los gráficos de alta resolución (`DATA/VBLK-0`, los que usa CorsixTH) y compara píxel a píxel:

```bash
tools/oraculo_mapa/oraculo.sh 1 "i;71,71;57,50;83,58;0,0"   # nivel, casillas en el centro («i» = cámara inicial)
```

| Fichero | Qué hace |
|---|---|
| `oraculo.sh <nivel> [casillas] [dir]` | Todo el proceso para un nivel. Deja en `dir` (por defecto `$TH64_WORK/resultados/hito2/vista`) las capturas, las vistas y las diferencias en rojo |
| `vblk.py <salida.tsh> <paleta.dat>` | `DATA/VBLK-0` a TSH1 y `DATA/MPALETTE.DAT` descomprimido |
| `comparar.py <captura.png> <vista.ppm> [dif.png] [--mezcla m.pgm] [--excluir x0,y0,x1,y1]` | Píxeles iguales, a 2 o menos (el redondeo de la mezcla de las sombras) y distintos, con las zonas distintas agrupadas |

Lo que se espera: ningún píxel distinto salvo las entidades que el motor aún no dibuja (puertas, personal y las sombras que proyectan las puertas), y diferencias de 1 o 2 en los píxeles de sombra. Resultados en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#hito-2--mapa-isométrico-con-scroll-a-30-fps).

La comparación entre lo que dibuja el RDP y el rasterizador de referencia no está aquí: la hace la ROM de pruebas en la N64 (`tools/motor_check.sh`).
