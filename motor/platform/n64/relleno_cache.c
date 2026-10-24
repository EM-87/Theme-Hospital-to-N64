// Relleno entre el código del motor y el de libdragon, que va detrás de todo.
// El bucle del dibujo (dibujar, mapa_n64_dibujar) llama a rdpq en cada sprite:
// si comparten líneas de la caché de instrucciones (16 KiB, correspondencia
// directa), se echan una a otra en cada llamada y los suelos tardan el doble.
// tools/cache_instrucciones.py dice si chocan y cuánto relleno lo evita; el
// fichero tiene que enlazarse después de mapa_n64.c.
#if defined(TH_PRUEBAS)
#define RELLENO "2400"
#elif defined(TH_HOSPITAL_HORAS)
#define RELLENO "192"
#else
#define RELLENO "192"
#endif

// Una función (main la llama una vez) para que --gc-sections no lo quite;
// salta por encima del relleno.
void relleno_cache(void)
{
    __asm__ volatile(".set push\n\t.set noreorder\n\t"
                     "b 1f\n\tnop\n\t"
                     ".space " RELLENO "\n"
                     "1:\n\t.set pop\n");
}
