# Herramientas de la fase 6

Comprobaciones de la interfaz del juego base ([`docs/06_plan.md`](../../docs/06_plan.md)). Resultados en [`docs/06_juego_base.md`](../../docs/06_juego_base.md).

```bash
source tools/env.sh
tools/hito6/panel.sh        # el panel inferior contra CorsixTH (unos 30 s)
tools/hito6/vistas.sh       # las cuatro vistas contra el hospital girado en CorsixTH (unos 2 minutos)
```

| Fichero | Qué hace |
|---|---|
| `panel.sh [dir]` | Hito 6.1. Captura el panel inferior de CorsixTH en 11 estados, lo dibuja igual con el motor a 640 × 480 y lo compara. Deja también los mismos estados a 320 × 240 con los gráficos de la N64 en `dir/baja` |
| [`bench/scenarios/captura_panel.lua`](../../bench/scenarios/captura_panel.lua) | Las capturas de CorsixTH: nivel 1 con solo el panel inferior, con dinero, fecha y reputación distintos, en pausa o no, con el puntero encima (los botones de gestión), con un botón pulsado y con una descripción. Escribe `capturas.txt` con cada estado |
| `vistas.sh [zonas] [dir]` | Hitos 6.2 y 6.2b. Monta en CorsixTH el hospital de prueba de las vistas (el nivel 1 con sus 8 salas) y lo lleva al motor como `motor/assets/escenarios/vistas.trg`, si no está. Después, para cada zona y cada una de las cuatro vistas, captura en CorsixTH el hospital girado, lo dibuja con el motor y lo compara, sin sombras en los dos (el motor, con `--sin-sombras`). Deja también la vista del motor con sombras, para mirarla |
| [`bench/scenarios/crear_vistas.lua`](../../bench/scenarios/crear_vistas.lua) | El hospital de prueba: nivel 1, las 8 salas que trae (con estantería, mesa de billar y una cama más), bancos, máquina de bebidas, extintor, plantas y papelera en el pasillo; corre 50 horas y guarda `vistas.sav` |
| [`bench/scenarios/captura_girada.lua`](../../bench/scenarios/captura_girada.lua) | Gira el hospital de una partida dentro de CorsixTH y hace la captura: lleva cada pieza del mapa a su sitio girado (las paredes, con las tablas de `walls/*.lua` de CorsixTH, y con la pareja en espejo si la luz de la vista lo pide), vuelve a crear los objetos con la orientación girada (o la opuesta, el sustituto) y quita las sombras, que CorsixTH solo sabe calcular con la luz girada. Es otra lógica que la del motor: la del motor busca, para cada casilla de la vista, lo que le toca |
| `comparar_giro.py <captura> <vista> <entidades> <mezcla> [dif]` | Compara por zonas: el mapa (igual, salvo algún píxel a 5 o menos en el borde de una sombra), las sombras (el redondeo de la mezcla, 5 o menos) y las entidades con 2 píxeles alrededor (el filtrado lineal de CorsixTH, 7 o menos) |
| `comparar_panel.py <dir> [--dif]` | Compara las capturas con lo que dibuja `th64sim panel alta`, donde pinta la interfaz. Las letras y los rectángulos tienen que ser iguales; las piezas del panel se cuentan por la mayor diferencia de un componente, porque CorsixTH las dibuja con filtrado lineal |

En la ROM de pruebas (`make -C motor PRUEBAS=1`):
- `comprobar_interfaz` dibuja el panel, su descripción y el puntero en 5 estados con el RDP y con el rasterizador de referencia, y los compara píxel a píxel (líneas `TH64 hito6.1`);
- el hospital del hito 4 se dibuja también en las otras tres vistas, contra la referencia, y `tools/motor_check.sh` comprueba que la lista de dibujo de cada una es la de `th64sim lista --giro=r` (líneas `TH64 hito6.2`).

En el PC, `th64sim vista`, `escena4` y `lista` aceptan `--giro=r` (la cámara girada r × 90° con el mismo centro), `--camara-girada` (la cámara que se da ya es la de la vista girada) y `--sin-sombras`; `th64sim tabla_giro` escribe la tabla de piezas giradas y la pareja en espejo de cada pared. Las sombras con la cámara girada (hito 6.2b) las comprueban las pruebas del núcleo (`motor/tests/test_vista.c`): las del mundo, en las mismas casillas en las cuatro vistas.
