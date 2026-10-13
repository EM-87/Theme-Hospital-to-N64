"""Fase 4b: qué parte del SoundFont usa la música de Theme Hospital.

1. Lee los .mid (convertidos de los XMI con el xmi2mid de CorsixTH) y saca los
   programas General MIDI usados por pieza (canal 10 = percusión, banco 128).
2. Lee el SF2 (phdr/pbag/pgen/inst/ibag/igen/shdr) y cuenta, para cada preset,
   regiones (zona de preset x zona de instrumento) y muestras distintas.
3. Con el coste en RAM de sf64_load medido en ares para dos bancos, ajusta
   bytes por región y por muestra y estima el banco recortado.
Uso: python3 -I banco.py <sf2> <mid>...
"""
import json
import struct
import sys


def midi_programs(path):
    d = open(path, "rb").read()
    assert d[:4] == b"MThd"
    ntrk = struct.unpack(">H", d[10:12])[0]
    pos = 8 + struct.unpack(">I", d[4:8])[0]
    progs = set()
    drums = False
    notes = 0
    for _ in range(ntrk):
        assert d[pos:pos + 4] == b"MTrk", d[pos:pos + 4]
        ln = struct.unpack(">I", d[pos + 4:pos + 8])[0]
        p, end = pos + 8, pos + 8 + ln
        status = 0
        while p < end:
            while d[p] & 0x80:  # delta
                p += 1
            p += 1
            b = d[p]
            if b == 0xFF:
                ln2, q = 0, p + 2
                while True:
                    ln2 = (ln2 << 7) | (d[q] & 0x7F)
                    q += 1
                    if not d[q - 1] & 0x80:
                        break
                p = q + ln2
                continue
            if b in (0xF0, 0xF7):
                ln2, q = 0, p + 1
                while True:
                    ln2 = (ln2 << 7) | (d[q] & 0x7F)
                    q += 1
                    if not d[q - 1] & 0x80:
                        break
                p = q + ln2
                continue
            if b & 0x80:
                status = b
                p += 1
            kind, ch = status & 0xF0, status & 0x0F
            n = 1 if kind in (0xC0, 0xD0) else 2
            args = d[p:p + n]
            p += n
            if kind == 0xC0 and ch != 9:
                progs.add(args[0])
            if kind == 0x90 and args[1] > 0:
                notes += 1
                if ch == 9:
                    drums = True
        pos = end
    return progs, drums, notes


def riff_chunks(d, off, end):
    while off < end:
        cid = d[off:off + 4]
        ln = struct.unpack("<I", d[off + 4:off + 8])[0]
        yield cid, off + 8, ln
        off += 8 + ln + (ln & 1)


def sf2_presets(path):
    d = open(path, "rb").read()
    assert d[:4] == b"RIFF" and d[8:12] == b"sfbk"
    pdta = {}
    for cid, off, ln in riff_chunks(d, 12, len(d)):
        if cid == b"LIST" and d[off:off + 4] == b"pdta":
            for c2, o2, l2 in riff_chunks(d, off + 4, off + ln):
                pdta[c2.decode()] = d[o2:o2 + l2]

    def recs(name, size, fmt):
        b = pdta[name]
        return [struct.unpack(fmt, b[i:i + size]) for i in range(0, len(b), size)]

    phdr = recs("phdr", 38, "<20sHHHIII")
    pbag = recs("pbag", 4, "<HH")
    pgen = recs("pgen", 4, "<HH")
    inst = recs("inst", 22, "<20sH")
    ibag = recs("ibag", 4, "<HH")
    igen = recs("igen", 4, "<HH")
    INSTRUMENT, SAMPLEID = 41, 53
    res = {}
    for i in range(len(phdr) - 1):
        name, prog, bank, bag0 = phdr[i][0].split(b"\0")[0].decode("latin-1"), phdr[i][1], phdr[i][2], phdr[i][3]
        bag1 = phdr[i + 1][3]
        regions, samples = 0, set()
        for b in range(bag0, bag1):
            g0, g1 = pbag[b][0], pbag[b + 1][0]
            inst_id = None
            for g in range(g0, g1):
                if pgen[g][0] == INSTRUMENT:
                    inst_id = pgen[g][1]
            if inst_id is None:
                continue  # zona global
            ib0, ib1 = inst[inst_id][1], inst[inst_id + 1][1]
            for ib in range(ib0, ib1):
                h0, h1 = ibag[ib][0], ibag[ib + 1][0]
                for g in range(h0, h1):
                    if igen[g][0] == SAMPLEID:
                        regions += 1
                        samples.add(igen[g][1])
        res[(bank, prog)] = {"name": name, "regions": regions, "samples": samples}
    return res


def main():
    sf2 = sys.argv[1]
    progs, drums, per_piece = set(), False, {}
    for m in sys.argv[2:]:
        p, dr, n = midi_programs(m)
        per_piece[m.rsplit("/", 1)[-1]] = {"programas": sorted(p), "percusion": dr, "notas": n}
        progs |= p
        drums |= dr
    pres = sf2_presets(sf2)
    all_regions = sum(v["regions"] for v in pres.values())
    all_samples = set().union(*(v["samples"] for v in pres.values()))
    used = [(0, p) for p in sorted(progs)] + ([(128, 0)] if drums else [])
    reg, smp = 0, set()
    for k in used:
        v = pres.get(k)
        if v is None:
            print("falta preset", k)
            continue
        reg += v["regions"]
        smp |= v["samples"]
    # Ajuste con lo medido en ares (sf64_load): TimGM6mb 361.840 B, Florestan 279.648 B
    out = {
        "piezas": per_piece,
        "programas_gm_usados": sorted(progs),
        "percusion": drums,
        "presets_usados": len(used),
        "banco": {"presets": len(pres), "regiones": all_regions, "muestras": len(all_samples)},
        "recortado": {"presets": len(used), "regiones": reg, "muestras": len(smp)},
    }
    print(json.dumps(out, indent=1, ensure_ascii=False))


if __name__ == "__main__":
    main()
