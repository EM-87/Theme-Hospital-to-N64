# Herramientas de arte

Inventarios del arte de Theme Hospital para el arte nuevo de TH64: las espaldas
de los objetos y el material que el juego trae y no usa. Leen los datos de
`TH_DATA_DIR` (`DATA`, alta resolución) y las definiciones de CorsixTH de
`CORSIXTH_DIR` (`source tools/env.sh`). No escriben nada en el repositorio:
el arte del juego y lo que se derive de él no se sube.

| Script | Qué hace |
|---|---|
| `espaldas.py [salida.tsv]` | Los 37 objetos que TH solo dibujó en norte y este: por objeto, las animaciones de reposo y de uso, sus fotogramas y los sprites distintos. Es lo que habría que dibujar para verlos de espaldas |
| `sin_usar.py <salida.tsv> [--hoja hoja.png]` | Las animaciones con fotogramas propios cuyo número no sale en el Lua de CorsixTH; con `--hoja`, una hoja de contacto con el fotograma central de cada una |
| `comun.py` | Lectura de las animaciones de `DATA` |
