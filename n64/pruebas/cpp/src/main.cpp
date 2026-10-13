// Prueba de C++ en la N64: excepciones, RTTI y libstdc++ (variantes por macros).
#include <libdragon.h>
#include <cstdint>
#include <typeinfo>
#if USE_STL
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>
#endif
#if USE_EXC
#include <stdexcept>
#endif
#if USE_IOSTREAM
#include <iostream>
#endif

struct Entidad { virtual ~Entidad() {} virtual int coste() const { return 1; } int id = 0; };
struct Paciente : Entidad { int coste() const override { return 3; } };
struct Medico : Entidad { int coste() const override { return 7; } };

static Entidad *crear(int i) { if (i & 1) return new Paciente; return new Medico; }

#if USE_EXC
static int lanza(int x) { if (x > 2) throw std::runtime_error("x demasiado grande"); return x; }
#endif

int main()
{
    debug_init_emulog();
    int total = 0;
#if USE_STL
    std::vector<std::unique_ptr<Entidad>> v;
    for (int i = 0; i < 10; i++) v.emplace_back(crear(i));
    std::unordered_map<std::string, int> m;
    for (auto &e : v) { total += e->coste(); m[std::string("k") + std::to_string(e->coste())]++; }
    std::sort(v.begin(), v.end(), [](auto &a, auto &b){ return a->coste() < b->coste(); });
    debugf("TH64 cpp: STL total=%d claves=%d\n", total, (int)m.size());
#else
    Entidad *arr[10];
    for (int i = 0; i < 10; i++) { arr[i] = crear(i); total += arr[i]->coste(); }
    debugf("TH64 cpp: sin STL total=%d\n", total);
    for (int i = 0; i < 10; i++) delete arr[i];
#endif
#if USE_RTTI
    Entidad *p = crear(1);
    debugf("TH64 cpp: RTTI typeid=%s dynamic_cast<Medico*>=%p dynamic_cast<Paciente*>=%s\n",
           typeid(*p).name(), (void*)dynamic_cast<Medico*>(p), dynamic_cast<Paciente*>(p) ? "ok" : "null");
    delete p;
#endif
#if USE_EXC
    try { lanza(1); lanza(5); debugf("TH64 cpp: no deberia llegar\n"); }
    catch (const std::exception &e) { debugf("TH64 cpp: excepcion capturada: %s\n", e.what()); }
#endif
#if USE_IOSTREAM
    std::cout << "TH64 cpp: iostream " << total << std::endl;
#endif
    debugf("TH64 cpp: fin\n");
#if SALIR
    emux_ioctl_exit();
#endif
    for (;;) {}
}
