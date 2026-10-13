// ROM de más de 64 MiB: ¿llega debugf con y sin modo homebrew (EMUX frente a ISViewer)?
#include <libdragon.h>
int main(void)
{
    bool ok = debug_init_emulog();
    dfs_init(DFS_DEFAULT_LOCATION);
    int fh = dfs_open("relleno.bin");
    debugf("TH64 grande: debug_init_emulog=%d emux=%lx, relleno.bin %d bytes\n", ok, emux_detect(1), dfs_size(fh));
    debugf("TH64 grande: fin\n");
    if (emux_detect(1)) emux_ioctl_exit();
    for (;;) {}
}
