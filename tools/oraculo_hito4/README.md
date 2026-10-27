# Oráculo del hito 4 (fase 5)

Escenario validado para comparar el bucle mínimo del motor con CorsixTH. El escenario es el nivel 1 con un mostrador de recepción, 6 bancos, una consulta de GP, una recepcionista y un médico. La comparación es estadística, con varias semillas.

```bash
source tools/env.sh
tools/oraculo_hito4/campana.sh                      # 10 semillas, 9.000 horas de juego cada una (unos 2 min)
tools/oraculo_hito4/campana.sh "1 2 3" 3000 /tmp/x  # semillas, horas y directorio a elegir
```

| Fichero | Qué hace |
|---|---|
| [`bench/scenarios/crear_hito4.lua`](../../bench/scenarios/crear_hito4.lua) | Crea la partida con el constructor del arnés: las mismas ventanas y validaciones que la interfaz. Sin emergencias, VIP, terremotos ni epidemias. Llegan 6 pacientes al mes (`--th64-ritmo`) y las enfermedades del nivel ya están descubiertas |
| [`bench/scenarios/traza_hito4.lua`](../../bench/scenarios/traza_hito4.lua) | Sigue a cada paciente y anota cada movimiento de dinero, sin gastar números aleatorios. El jugador responde cada fax en cuanto llega: arriesga un diagnóstico si puede y, si no, manda al paciente a casa |
| [`bench/scenarios/sonda_hospital.lua`](../../bench/scenarios/sonda_hospital.lua) | Foto del hospital cada N horas (pacientes, personal, colas), para ver dónde se atasca un escenario |
| [`bench/scenarios/exportar_escenario.lua`](../../bench/scenarios/exportar_escenario.lua) | Exporta una partida guardada para el motor: las 16.384 casillas (`celdas.tsv`) y el resto (`escenario.json`: fecha, llegadas, dinero, objetos, salas y personal) |
| `escenario.py <dir_exportado> <motor.tsv> <salida.trg>` | Pasa lo exportado a TRG1 para el motor, con solo las casillas que difieren del nivel recién cargado (`th64sim mapa ... --volcar motor.tsv`) |
| `campana.sh` | Crea y traza el escenario para cada semilla, y resume |
| `resumen.py <dir>...` | Cifras de cada semilla, con su media y su desviación típica. Incluye las horas-paciente vomitando y orinando (`h_vomit`, `h_pee`, de `acciones.tsv`) |
| `motor.sh [semillas] [horas] [repeticiones] [dir] [nombre]` | El motor contra CorsixTH: para cada semilla de la campaña exporta la partida, comprueba las 16.384 casillas y corre `th64sim hito4` con varias semillas propias (por defecto 10). Copia el escenario de la primera semilla a `motor/assets/escenarios/<nombre>.trg` (por defecto `hito4`, el que llevan las ROM; `hito5` es el de [`tools/hito5`](../hito5/README.md); con `-` no copia nada) y acaba con `comparar.py` |
| `comparar.py <corsixth.json> <motor.json>` | Compara las cifras de los dos lados con la t de Welch y marca las que tienen \|t\| ≥ 2 |
| [`bench/scenarios/captura_escenario.lua`](../../bench/scenarios/captura_escenario.lua) | Captura de 640 × 480 de la partida, parada y sin personas, para comparar el dibujo de los objetos con `th64sim escena4` |
| [`bench/scenarios/sonda_gp.lua`](../../bench/scenarios/sonda_gp.lua) | Qué hace el médico cada hora y cuándo se sienta y se despacha cada paciente en la consulta (`medico.tsv`, `visitas.tsv`). El motor escribe lo mismo con `TH64_SONDA_GP=<dir> th64sim hito4 ...` |
| [`bench/scenarios/sonda_paciente.lua`](../../bench/scenarios/sonda_paciente.lua) | La cola de acciones de un paciente hora a hora entre dos horas (`--th64-id`, `--th64-desde`, `--th64-hasta`). En el motor, `TH64_DEPURAR=<id>` |

Las sondas abren y contestan los faxes igual que `traza_hito4.lua`, y no gastan números aleatorios: la partida es la misma que la de la campaña (lo comprueban el saldo final y la huella del generador).

Para comparar el motor con una campaña de 40 semillas (la campaña tarda unos 10 min; `motor.sh`, 3,5 min para 400 corridas de 9.000 horas):

```bash
tools/oraculo_hito4/campana.sh "$(seq -s ' ' 1 40)" 9000
tools/oraculo_hito4/motor.sh "$(seq -s ' ' 1 40)" 9000 10
```

Para comparar el dibujo de los objetos (alta resolución de `tools/oraculo_mapa/alta.py`):

```bash
O=$TH64_WORK/resultados/hito4/objetos A=$TH64_WORK/resultados/alta E=$TH64_WORK/resultados/hito4/campana/escenario_1
CORSIXTH_SAVES=$TH64_WORK/resultados/hito4/campana/saves tools/bench_run.sh bench/scenarios/captura_escenario.lua \
  $O/corsixth_1 --load=hito4_1.sav --th64-png=$O/escena_1.png
motor/build/pc/th64sim escena4 motor/assets $E/escenario.trg $A/vblk-0.tsh $A/vspr-0.tsh $A/animaciones.tan \
  $A/mpalette.dat 32 -704 1936 640 480 0 0 $O/escena_1.ppm $O/escena_1_mezcla.pgm $O/escena_1_entidades.pgm
python3 -I tools/oraculo_mapa/comparar.py $O/escena_1.png $O/escena_1.ppm $O/dif.png --mezcla $O/escena_1_mezcla.pgm \
  --entidades $O/escena_1_entidades.pgm
```

Para llevar la partida de una semilla al motor y comprobar que el mapa queda igual que en CorsixTH:

```bash
E=$TH64_WORK/resultados/hito4/escenario_1
CORSIXTH_SAVES=$TH64_WORK/resultados/hito4/campana/saves \
  tools/bench_run.sh bench/scenarios/exportar_escenario.lua $E --load=hito4_1.sav
motor/build/pc/th64sim mapa motor/assets/niveles/nivel01.tmp 1 --volcar /tmp/motor.tsv
python3 -I tools/oraculo_hito4/escenario.py $E /tmp/motor.tsv $E/escenario.trg
motor/build/pc/th64sim escenario motor/assets/niveles/nivel01.tmp $E/escenario.trg $E/celdas.tsv
```

Ficheros de la traza, por semilla:
- `pacientes.tsv`: la hora de cada paso de cada paciente;
- `dinero.tsv`: cada movimiento de dinero;
- `dias.tsv`: saldo y pacientes en cada fase, cada 50 horas;
- `acciones.tsv`: horas-paciente pasadas en cada acción.

Resultados y criterios de validez en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#escenario-del-hito-4); la comparación con el motor, en [el hito 4](../../docs/05_prototipo.md#hito-4--bucle-mínimo).
