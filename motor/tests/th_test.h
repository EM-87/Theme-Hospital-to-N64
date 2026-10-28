// Marco mínimo de pruebas: compila igual en el PC y en la N64.
// La salida pasa por th_test_log, que pone quien enlaza (printf o debugf).
#ifndef TH_TEST_H
#define TH_TEST_H

#include <stdint.h>

extern void (*th_test_log)(const char *fmt, ...);
extern int th_test_failures;
extern int th_test_checks;

#define TH_CHECK(cond)                                                   \
    do {                                                                 \
        th_test_checks++;                                                \
        if (!(cond)) {                                                   \
            th_test_failures++;                                          \
            th_test_log("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                \
    } while (0)

#define TH_CHECK_EQ(a, b)                                                          \
    do {                                                                           \
        long long va_ = (long long)(a), vb_ = (long long)(b);                      \
        th_test_checks++;                                                          \
        if (va_ != vb_) {                                                          \
            th_test_failures++;                                                    \
            th_test_log("FALLO %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, \
                        #a, #b, va_, vb_);                                         \
        }                                                                          \
    } while (0)

// Ejecuta todas las pruebas del núcleo; devuelve el número de fallos.
int th_run_core_tests(void);
// Pruebas del mapa (test_map.c) y del A*, las animaciones y los humanoides (test_humanoid.c).
void th_test_map(void);
void th_test_humanoid(void);
// Pruebas del lector de reglas (test_rules.c).
void th_test_rules(void);
// Pruebas de las cuatro vistas (test_vista.c).
void th_test_vista(void);
// Simulación de referencia: huella del estado tras 'ticks' pasos.
uint32_t th_golden_sim(uint32_t seed, int ticks);
// Lo mismo con float (informativo: ¿coincide entre plataformas?).
uint32_t th_golden_sim_float(uint32_t seed, int ticks);

#endif
