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
| `resumen.py <dir>...` | Cifras de cada semilla, con su media y su desviación típica |

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

Resultados y criterios de validez en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#escenario-del-hito-4).
