// Prueba de guardado: detecta EEPROM / SRAM / FlashRAM / Controller Pak, lee un
// contador de arranques y lo incrementa, para comprobar persistencia en ares.
#include <libdragon.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef struct { char magic[8]; uint32_t arranques; uint32_t pad; } rec_t;   // 16 B

static void informa(const char *medio, int capacidad, rec_t *r, bool nuevo)
{
    debugf("TH64 save: %-9s capacidad %6d B, %s arranques=%lu\n", medio, capacidad,
           nuevo ? "vacio," : "leido,", r->arranques);
}

int main(void)
{
    debug_init_emulog();
    rec_t r __attribute__((aligned(16)));
    int hay = 0;

    eeprom_type_t et = eeprom_present();
    if (et != EEPROM_NONE) {
        int cap = eeprom_total_blocks() * EEPROM_BLOCK_SIZE;
        eeprom_read_bytes(&r, 0, sizeof r);
        bool nuevo = memcmp(r.magic, "TH64SAVE", 8) != 0;
        if (nuevo) { memcpy(r.magic, "TH64SAVE", 8); r.arranques = 0; }
        informa("EEPROM", cap, &r, nuevo);
        r.arranques++; eeprom_write_bytes(&r, 0, sizeof r); eeprom_wait_idle(); hay++;
    }
    sram_init();
    int sz = sram_detect();
    if (sz > 0) {
        sram_read(&r, 0, sizeof r);
        bool nuevo = memcmp(r.magic, "TH64SAVE", 8) != 0;
        if (nuevo) { memcpy(r.magic, "TH64SAVE", 8); r.arranques = 0; }
        informa("SRAM", sz, &r, nuevo);
        r.arranques++; sram_write(&r, 0, sizeof r); hay++;
    }
    flashram_info_t fi;
    if (flashram_init(NULL, &fi)) {
        flashram_read(&r, 0, sizeof r);
        bool nuevo = memcmp(r.magic, "TH64SAVE", 8) != 0;
        if (nuevo) { memcpy(r.magic, "TH64SAVE", 8); r.arranques = 0; }
        informa("FlashRAM", fi.total_size, &r, nuevo);
        debugf("TH64 save: FlashRAM chip %s, sector %d B, pagina %d B\n", fi.name, fi.sector_size, fi.page_size);
        r.arranques++; flashram_write(&r, 0, sizeof r); hay++;
    }
    joypad_init();
    for (int i = 0; i < 10; i++) { joypad_poll(); wait_ms(20); }
    joypad_accessory_type_t acc = joypad_get_accessory_type(JOYPAD_PORT_1);
    debugf("TH64 save: mando 1 accesorio=%d (cpak=%d)\n", acc, JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK);
    if (acc == JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK) {
        if (cpakfs_mount(JOYPAD_PORT_1, "cpak1:/") < 0) {
            debugf("TH64 save: cpak sin formato, se formatea\n");
            cpakfs_format(JOYPAD_PORT_1, false);
            cpakfs_mount(JOYPAD_PORT_1, "cpak1:/");
        }
        cpakfs_stats_t st; cpakfs_get_stats(JOYPAD_PORT_1, &st);
        FILE *f = fopen("cpak1:/TH64.01-PARTIDA.A", "rb");
        bool nuevo = true;
        if (f) { nuevo = fread(&r, sizeof r, 1, f) != 1; fclose(f); }
        if (nuevo) { memcpy(r.magic, "TH64SAVE", 8); r.arranques = 0; }
        informa("CPak", st.pages.total * 256, &r, nuevo);
        debugf("TH64 save: CPak paginas usadas %d de %d, notas %d\n", st.pages.used, st.pages.total, st.notes.used);
        r.arranques++;
        f = fopen("cpak1:/TH64.01-PARTIDA.A", "wb");
        if (f) { fwrite(&r, sizeof r, 1, f); fclose(f); } else debugf("TH64 save: fopen cpak wb fallo errno=%d\n", errno);
        cpakfs_unmount(JOYPAD_PORT_1);
        hay++;
    }
    debugf("TH64 save: medios encontrados %d\n", hay);
    debugf("TH64 save: fin\n");
    if (emux_detect(1)) emux_ioctl_exit();
    for (;;) {}
}
