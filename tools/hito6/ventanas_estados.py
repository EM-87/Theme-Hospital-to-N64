"""Los estados de las ventanas de la ROM contra los de th64sim ventanas.

Uso: python3 -I tools/hito6/ventanas_estados.py <isviewer.log> <dir> <ventanas.txt>

<dir> es donde escribió th64sim ventanas baja (con ventanas.txt): su salida
en <dir>/salida.txt y el registro de la partida de cada estado en
<dir>/<estado>.log. Para cada estado de ventanas.txt compara, en el registro
de la ROM de pruebas (comprobar_ventanas, platform/n64/main.c), entre
«TH64 demo5: estado <nombre>: empieza» y la línea del estado:
  - la huella de la lista de dibujo entera (mapa e interfaz con el puntero,
    th_drawlist_hash: «lista XXXXXXXX») con la «huella» de la línea del
    estado de th64sim;
  - las líneas «TH64 orden» y «TH64 partida: H horas, huella, saldo» con las
    de <estado>.log, sin la de partida nueva (la ROM no la escribe en los
    estados).
La comparación de píxeles de la ROM es contra su propia referencia, pintada
con la misma lista: esto comprueba que la ROM llega al mismo estado y a la
misma lista que el PC. Sale con 1 si algún estado es distinto o falta.
"""
import os
import re
import sys


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    log = [l.rstrip("\r\n") for l in open(sys.argv[1], encoding="utf-8", errors="replace")]
    d = sys.argv[2]
    nombres = [l.split()[0] for l in open(sys.argv[3], encoding="utf-8") if l.strip() and not l.startswith("#")]
    pc = {}
    for l in open(os.path.join(d, "salida.txt"), encoding="utf-8"):
        m = re.match(r"(\S+): ventana .*huella ([0-9a-f]{8})$", l.rstrip("\n"))
        if m:
            pc[m.group(1)] = m.group(2)
    fallos = 0
    for nombre in nombres:
        marca = f"TH64 demo5: estado {nombre}: "
        i = next((k for k, l in enumerate(log) if marca + "empieza" in l), None)
        j = None if i is None else next((k for k in range(i + 1, len(log)) if marca in log[k]), None)
        if j is None:
            print(f"   {nombre}: no está en el registro de la ROM")
            fallos += 1
            continue
        m = re.search(r"elementos, lista ([0-9a-f]{8})", log[j])
        lista = m.group(1) if m else "?"
        rom = [l[l.find("TH64 "):] for l in log[i + 1:j]
               if "TH64 orden\t" in l or re.search(r"TH64 partida: \d+ horas, huella", l)]
        txt = [l.rstrip("\n") for l in open(os.path.join(d, nombre + ".log"), encoding="utf-8")][1:]
        ok = lista == pc.get(nombre) and rom == txt
        fallos += not ok
        final = txt[-1].replace("TH64 partida: ", "") if txt else "?"
        print(f"   {nombre}: lista {lista} (PC {pc.get(nombre)}); {len(txt) - 1} órdenes, {final}: "
              f"{'igual' if ok else 'DISTINTO'}")
        if rom != txt:
            print(f"     ROM: {rom}\n     PC:  {txt}")
    print(f"   {len(nombres) - fallos} de {len(nombres)} estados iguales en la ROM y en th64sim ventanas")
    sys.exit(1 if fallos else 0)


if __name__ == "__main__":
    main()
