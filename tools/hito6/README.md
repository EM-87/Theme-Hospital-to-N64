# Herramientas de la fase 6

Comprobaciones de la interfaz del juego base ([`docs/06_plan.md`](../../docs/06_plan.md)). Resultados en [`docs/06_juego_base.md`](../../docs/06_juego_base.md).

```bash
source tools/env.sh
tools/hito6/panel.sh        # el panel inferior contra CorsixTH (unos 30 s)
```

| Fichero | Qué hace |
|---|---|
| `panel.sh [dir]` | Hito 6.1. Captura el panel inferior de CorsixTH en 11 estados, lo dibuja igual con el motor a 640 × 480 y lo compara. Deja también los mismos estados a 320 × 240 con los gráficos de la N64 en `dir/baja` |
| [`bench/scenarios/captura_panel.lua`](../../bench/scenarios/captura_panel.lua) | Las capturas de CorsixTH: nivel 1 con solo el panel inferior, con dinero, fecha y reputación distintos, en pausa o no, con el puntero encima (los botones de gestión), con un botón pulsado y con una descripción. Escribe `capturas.txt` con cada estado |
| `comparar_panel.py <dir> [--dif]` | Compara las capturas con lo que dibuja `th64sim panel alta`, donde pinta la interfaz. Las letras y los rectángulos tienen que ser iguales; las piezas del panel se cuentan por la mayor diferencia de un componente, porque CorsixTH las dibuja con filtrado lineal |

En la ROM de pruebas (`make -C motor PRUEBAS=1`), `comprobar_interfaz` dibuja el panel, su descripción y el puntero en 5 estados con el RDP y con el rasterizador de referencia, y los compara píxel a píxel (líneas `TH64 hito6.1`).
