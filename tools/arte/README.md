# Herramientas de arte

Inventarios del arte de Theme Hospital para el arte nuevo de TH64 (las espaldas
de los objetos y el material que el juego trae y no usa) y el importador del
arte nuevo. Leen los datos de
`TH_DATA_DIR` (`DATA`, alta resolución; el importador, también `DATAM`) y las
definiciones de CorsixTH de `CORSIXTH_DIR` (el importador, con `lua5.4`;
`source tools/env.sh`). No escriben nada en el repositorio: el arte del juego y
lo que se derive de él no se sube.

| Script | Qué hace |
|---|---|
| `espaldas.py [salida.tsv]` | Los 37 objetos que TH solo dibujó en norte y este: por objeto, las animaciones de reposo y de uso, sus fotogramas y los sprites distintos. Es lo que habría que dibujar para verlos de espaldas |
| `sin_usar.py <salida.tsv> [--hoja hoja.png]` | Las animaciones con fotogramas propios cuyo número no sale en el Lua de CorsixTH; con `--hoja`, una hoja de contacto con el fotograma central de cada una |
| `importar.py [--vistas dir] [--comprobar]` | El importador de arte nuevo (abajo): convierte las vistas del manifiesto y, con `--vistas`, deja para cada una la de hoy junto a la nueva. Con `--comprobar`, la vuelta completa con el arte de TH (abajo). Necesita numpy y Pillow |
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
  Hace falta una de las dos, y la que falte sale de la otra, sin sus motas: la
  baja, de la alta recortada a su caja con lados pares y reducida a la mitad
  exacta (no depende del margen del PNG); la alta, de la baja ampliada al doble.
- El dibujo puede traer la sombra pintada, opaca. Se quita si el objeto tiene
  sombra de suelo en TH: 6 de los 59 objetos con animación de reposo (24 de sus
  216 orientaciones). En los otros 48 que se importan no hay huella con la que
  reconocerla y se queda, en los colores del objeto: hay que mirar la vista
  previa y, si sobra, borrarla del PNG.
- Las partes semitransparentes que no son sombra (el cristal de las puertas de
  la entrada y de vaivén, y los rotores del helicóptero: 10 orientaciones de 5
  objetos) todavía no se saben convertir: esas vistas se rechazan con un error.
- El dibujo tiene que estar a la escala de su resolución: si mide menos de
  1/1,5 o más de 1,5 veces lo que el juego dibuja hoy, da error. Las motas
  sueltas del PNG (manchas de hasta 16 píxeles a más de 6 de las manchas del
  dibujo, contadas en el PNG que se da) se quitan y el informe las cuenta.

`tools/conv/conv.py` lo mete en los assets del motor si hay manifiesto:
1. **la hoja `MSPR-0`** lleva los sprites nuevos al final;
2. **las tablas de animación** llevan una animación nueva por vista, también al
   final: no cambia ningún índice;
3. **las reglas** llevan en cada objeto `arte` = {orientación: animación}.

Los tres pasos (`graficos`, `animaciones` y `reglas`) van siempre juntos: pedir
uno ejecuta los tres, para que salgan del mismo manifiesto. Por eso los tres
necesitan siempre CorsixTH y `lua5.4`, como las reglas; con arte, además numpy y
Pillow. Lo que falte se dice antes de escribir nada, y un error del manifiesto o
de un PNG (que no está, roto o enorme) sale en una línea, con el objeto, la
orientación, la resolución y, si es del PNG, su ruta.

La vista (`motor/vista/objetos.c`) dibuja esa animación sin espejo, si está en
las tablas cargadas. Los gráficos de alta resolución del oráculo de CorsixTH
(`tools/oraculo_mapa/alta.py`) se hacen sin arte, salvo con `--arte`, así que
el oráculo de gráficos no cambia. `tools/conv/comparar_reglas.py` no cuenta la
clave `arte`, que CorsixTH no tiene.

Cómo se convierte cada vista (en cada resolución, ver `importar.py`):
1. **la referencia** es lo que el juego dibuja hoy: la animación de reposo de esa
   orientación o la de su espejo. Del dibujo se quitan las motas y se comprueba
   la escala;
2. **el color de sombra**: los píxeles más cerca del gris de la sombra de la
   referencia que de cualquier color opaco del objeto;
3. **la posición** es la que más casa con la silueta de la referencia, sin lo que
   tiene color de sombra;
4. **la sombra pintada** se quita: las manchas de color de sombra que tocan la
   huella de la sombra de la referencia, enteras, y las migas sueltas de su borde
   (hasta 3 píxeles; 12 si la imagen sale de la baja ampliada al doble). Queda la
   de la referencia, semitransparente. Si el objeto
   usa el mismo gris que su sombra (la «sombra ambigua» del informe), lo que cae
   sobre la silueta de la referencia se queda;
5. **los colores** pasan a los de los sprites opacos del objeto.

**La vuelta completa** (`importar.py --comprobar`) prueba el importador con el
arte de TH: cada orientación de cada objeto, con su sombra pintada en opaco,
pasa por el importador como si fuera arte nuevo y tiene que salir igual, píxel a
píxel y en el mismo sitio. Lo hace con el dibujo tal cual; con lo que trae el
arte de verdad (el borde de la sombra hasta 3 píxeles fuera de su huella, migas
sueltas de 1 a 3 píxeles junto a él y una mota lejos del dibujo); en `DATAM`,
con ese dibujo sacado al doble, como una alta que sale de la baja; y al doble y
a la mitad, que tienen que dar el error de escala. Salen bien las 206
orientaciones que se importan, en todas las pasadas, en `DATA` y en `DATAM`; las
otras 10, con cristal o rotores, se rechazan. Estropeando el importador a
propósito, lo nota todo menos una cosa: dónde se coloca un dibujo distinto de la
referencia, porque con el mismo dibujo el sitio bueno gana siempre. Eso lo dice
el escritorio (`docs/06_juego_base.md`).

**Lo que no cubre todavía.** Mientras alguien usa un objeto, el objeto no se
dibuja: lo dibuja la animación de la persona, que lo lleva dentro. Por ejemplo,
las del médico y la enfermera sentados al escritorio llevan sus sprites. Así que
el arte nuevo solo se ve con el objeto libre; con alguien usándolo, vuelve el
espejo. Tampoco pasan a la animación nueva los elementos con capa, como la luz
del visor de rayos X durante la operación, ni se convierten las partes
semitransparentes que no son sombra (arriba).
