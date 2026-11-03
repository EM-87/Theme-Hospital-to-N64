# Herramientas de arte

Inventarios del arte de Theme Hospital para el arte nuevo de TH64 (las espaldas
de los objetos y el material que el juego trae y no usa) y el importador del
arte nuevo. Leen los datos de
`TH_DATA_DIR` (`DATA`, alta resolución) y las definiciones de CorsixTH de
`CORSIXTH_DIR` (`source tools/env.sh`). No escriben nada en el repositorio:
el arte del juego y lo que se derive de él no se sube.

| Script | Qué hace |
|---|---|
| `espaldas.py [salida.tsv]` | Los 37 objetos que TH solo dibujó en norte y este: por objeto, las animaciones de reposo y de uso, sus fotogramas y los sprites distintos. Es lo que habría que dibujar para verlos de espaldas |
| `sin_usar.py <salida.tsv> [--hoja hoja.png]` | Las animaciones con fotogramas propios cuyo número no sale en el Lua de CorsixTH; con `--hoja`, una hoja de contacto con el fotograma central de cada una |
| `importar.py [--vistas dir]` | El importador de arte nuevo (abajo): convierte las vistas del manifiesto y, con `--vistas`, deja para cada una la de hoy junto a la nueva |
| `comun.py` | Lectura de las animaciones de `DATA` |

## El arte nuevo

Las vistas que faltan (las espaldas, o las que el juego dibuja en espejo) se
hacen fuera del repositorio: a mano o con IA (PixelLab, ver
[`docs/06_plan.md`](../../docs/06_plan.md#arte-nuevo-las-espaldas-con-ia)). Se
dejan en `TH64_ARTE_DIR` (por defecto `$TH64_WORK/arte`) con un manifiesto
`arte.json`:

```json
{"objetos": [
  {"objeto": "desk", "orientacion": "south", "alta": "desk/sur_alta.png",
   "origen": "PixelLab, rotación de 8 vistas"}
]}
```

- `objeto`: el id de CorsixTH (`objects/*.lua`); `orientacion`: `north`, `east`,
  `south` o `west`.
- `alta` y `baja`: PNG con fondo transparente a la escala de `DATA` y de `DATAM`.
  Si falta la baja, sale de la alta reducida a la mitad.
- El dibujo puede traer la sombra pintada: se quita sola.

`tools/conv/conv.py` lo mete en los assets del motor si hay manifiesto:
1. **la hoja `MSPR-0`** lleva los sprites nuevos al final;
2. **las tablas de animación** llevan una animación nueva por vista, también al
   final: no cambia ningún índice;
3. **las reglas** llevan en cada objeto `arte` = {orientación: animación}.

La vista (`motor/vista/objetos.c`) dibuja esa animación sin espejo, si está en
las tablas cargadas. Los gráficos de alta resolución del oráculo de CorsixTH
(`tools/oraculo_mapa/alta.py`) se hacen sin arte, salvo con `--arte`, así que
las comparaciones con CorsixTH no cambian.

Cómo se convierte cada vista (en cada resolución, ver `importar.py`):
1. **la referencia** es lo que el juego dibuja hoy: la animación de reposo de esa
   orientación o la de su espejo;
2. **los colores** pasan a los de los sprites opacos del objeto;
3. **la sombra pintada** (opaca) se quita, y se pone la de la referencia,
   semitransparente;
4. **la posición** es la que más casa con la silueta de la referencia.

**Lo que no cubre todavía.** Mientras alguien usa un objeto, el objeto no se
dibuja: lo dibuja la animación de la persona, que lo lleva dentro. Por ejemplo,
las del médico y la enfermera sentados al escritorio llevan sus sprites. Así que
el arte nuevo solo se ve con el objeto libre; con alguien usándolo, vuelve el
espejo.
