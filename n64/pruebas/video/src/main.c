// Prueba de vídeo para TH64: fmv_play con H.264 + audio wav64.
// Mide RAM (heap antes, pico durante y después), fotogramas mostrados y
// saltados, y fps efectivos, en dos modos:
//  A) fmv_play configura su propio display (32 bpp, 2 búferes: fmv.c:35-41);
//  B) el juego ya tiene su display (320x240, 16 bpp, 3 búferes) y se pasa
//     disable_display_init = true, como haría el motor entre pantallas.
#include <libdragon.h>

static int heap_used(void)
{
    heap_stats_t h;
    sys_get_heap_stats(&h);
    return h.used;
}

typedef struct { int rendered, last_idx, peak; uint32_t t0; } stats_t;

static void osd(void *ctx, int frame_idx, float time_sec, fmv_control_t *ctrl)
{
    stats_t *st = ctx;
    if (st->rendered == 0) st->t0 = get_ticks();
    st->rendered++;
    st->last_idx = frame_idx;
    int u = heap_used();
    if (u > st->peak) st->peak = u;
}

static void prueba(const char *fn, bool game_display, int ahead)
{
    stats_t st = {0};
    int h0 = heap_used();
    st.peak = h0;
    uint32_t t0 = get_ticks();
    fmv_play(fn, &(fmv_parms_t){
        .osd_callback = osd, .osd_ctx = &st,
        .disable_display_init = game_display,
        .decode_ahead_pics = ahead,
    });
    uint32_t t1 = get_ticks();
    int h1 = heap_used();
    float secs = TICKS_DISTANCE(st.t0, t1) / (float)TICKS_PER_SECOND;
    debugf("TH64 video: %-22s %s ahead=%d | heap antes %d KiB, pico +%d KiB, después %+d KiB | "
           "mostrados %d de %d fotogramas (%d saltados) en %.2f s = %.2f fps | total %.2f s\n",
           fn, game_display ? "display del juego 16bpp" : "display propio de fmv", ahead,
           h0 / 1024, (st.peak - h0) / 1024, (h1 - h0) / 1024,
           st.rendered, st.last_idx + 1, st.last_idx + 1 - st.rendered, secs,
           st.rendered / secs, TICKS_DISTANCE(t0, t1) / (float)TICKS_PER_SECOND);
}

int main(void)
{
    debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    rdpq_init();
    audio_init(32000, 4);
    mixer_init(8);
    video_register_codec(&h264_codec);
    debugf("TH64 video: RDRAM %d KiB; heap tras rdpq+audio+mixer: %d KiB\n",
           get_memory_size() / 1024, heap_used() / 1024);

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_RESAMPLE);
    debugf("TH64 video: display del juego 320x240x16, 3 búferes: heap %d KiB\n", heap_used() / 1024);
    prueba("rom:/test25.h264", true, 2);
    prueba("rom:/test25.h264", true, 4);
    debugf("TH64 video: fin\n");
    for (;;) {
        surface_t *d = display_get();
        rdpq_attach_clear(d, NULL);
        rdpq_detach_show();
    }
}
