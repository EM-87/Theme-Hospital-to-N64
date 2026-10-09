// Convierte XMI (música de Theme Hospital) a MIDI estándar con el mismo código
// que usa CorsixTH (CorsixTH/Src/xmi2mid.cpp). Uso: xmi2mid <entrada.xmi> <salida.mid>
// Compilar: ver tools/datos/musica.py, que lo construye si no existe.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "xmi2mid.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "uso: %s <entrada.xmi> <salida.mid>\n", argv[0]);
    return 2;
  }
  std::FILE* f = std::fopen(argv[1], "rb");
  if (!f) return 1;
  std::vector<unsigned char> xmi;
  unsigned char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) xmi.insert(xmi.end(), buf, buf + n);
  std::fclose(f);
  size_t len = 0;
  uint8_t* mid = transcode_xmi_to_midi(xmi.data(), xmi.size(), &len);
  if (!mid) return 1;
  f = std::fopen(argv[2], "wb");
  if (!f) return 1;
  std::fwrite(mid, 1, len, f);
  std::fclose(f);
  return 0;
}
