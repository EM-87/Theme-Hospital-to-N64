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
| `campana.sh` | Crea y traza el escenario para cada semilla, y resume |
| `resumen.py <dir>...` | Cifras de cada semilla, con su media y su desviación típica |

Ficheros de la traza, por semilla:
- `pacientes.tsv`: la hora de cada paso de cada paciente;
- `dinero.tsv`: cada movimiento de dinero;
- `dias.tsv`: saldo y pacientes en cada fase, cada 50 horas;
- `acciones.tsv`: horas-paciente pasadas en cada acción.

Resultados y criterios de validez en [`docs/05_prototipo.md`](../../docs/05_prototipo.md#escenario-del-hito-4).
