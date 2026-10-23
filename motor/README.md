# Motor de Theme Hospital 64

Motor propio en C sobre libdragon (fase 5). La lógica de CorsixTH v0.70.1 sirve de especificación; ver [`docs/04_decision.md`](../docs/04_decision.md) y [`docs/04_construccion.md`](../docs/04_construccion.md).

| Ruta | Contenido |
|---|---|
| `core/` | Núcleo de la simulación en C portable (gnu17): sin libdragon, solo enteros y coma fija Q16.16, pools de tamaño fijo. Compila igual en el PC y en la N64 |
| `platform/n64/` | Capa de la consola: vídeo, RDP, mandos, sonido, ficheros |
| `platform/pc/` | `th64sim`, el núcleo en el PC sin pantalla |
| `tests/` | Pruebas del núcleo (`th_test.h`), las mismas en el PC y en la ROM |
| `filesystem/` | Assets de la ROM. Los genera el conversor a partir de los datos del juego de cada usuario; no están en el repositorio |

```bash
source tools/env.sh
tools/motor_check.sh        # todo: PC, PC con sanitizers, pruebas en ares, ROM
make -C motor               # ROM: motor/th64.z64
make -C motor PRUEBAS=1     # ROM con las pruebas del núcleo: motor/th64_pruebas.z64
cmake -S motor -B motor/build/pc -G Ninja && cmake --build motor/build/pc && ctest --test-dir motor/build/pc
```

Reglas del núcleo:
- solo tipos de `stdint`, sin `long` ni `int` sin tamaño en lo que se guarda;
- sin `float` ni `double` en el estado de la simulación;
- recorridos en orden de índice;
- sin `malloc` después de iniciar.
