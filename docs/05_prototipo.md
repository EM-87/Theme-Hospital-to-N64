# Fase 5 — Prototipos en N64

Fecha de inicio: 2026-10-23. Estado: **en curso**. Plan: [`PLAN.md`](PLAN.md#fase-5--prototipos-en-n64-solo-tras-aprobar-la-fase-4), con los cambios de [`04_decision.md`](04_decision.md#cambios-al-plan-de-la-fase-5). Cada hito se cierra con un commit, una captura de ares y sus cifras aquí.

## Entorno del motor

**Hecho.** El código está en [`motor/`](../motor/README.md), y [`tools/motor_check.sh`](../tools/motor_check.sh) lo comprueba todo de una vez:

| Comprobación | Resultado |
|---|---|
| Núcleo en el PC con gcc (`-Wall -Wextra -Werror`) | 60.045 comprobaciones, 0 fallos, 0,05 s |
| Lo mismo con ASan y UBSan | 0 fallos |
| ROM de pruebas en ares (modo Homebrew, se cierra sola) | 60.045 comprobaciones, 0 fallos |
| Huella de la simulación de referencia (semilla 1, 10.000 pasos) | `d38fbaef` en el PC y en la N64 |
| ROM normal | Arranca: 8.192 KiB de RDRAM con Expansion Pak, heap usado 588 de 7.865 KiB, DFS correcto |

- **Tamaño.** La ROM ocupa 208 KiB. El código ocupa 245.404 B (`text`), casi todo libdragon: vídeo, RDP, texto y mandos.
- **Tiempo de compilación.** La ROM tarda 0,36 s desde cero y 0,32 s tras tocar un fichero.

![Pantalla de estado del motor en ares](img/05_entorno.png)

*Pantalla de estado de la ROM en ares. El «1 VPS» de la barra de ares se debe al vídeo por software del contenedor, no a la ROM.*

Punto de partida: el núcleo de ejemplo de la fase 4 (`n64/pruebas/nucleo`), con:
- el pool de entidades con asas;
- la coma fija Q16.16;
- el generador xorshift32;
- el marco de pruebas.

## Hito 0 — Conversor

Pendiente.
