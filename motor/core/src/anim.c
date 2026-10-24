#include "th/anim.h"

#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

int th_anims_init(th_anims *a, const uint8_t *data, size_t len)
{
    if (len < 36 || memcmp(data, "TAN1", 4) != 0)
        return -1;
    a->data = data;
    a->count = rd16(data + 4);
    a->frames = rd32(data + 8);
    a->list_len = rd32(data + 12);
    a->elements = rd32(data + 16);
    a->off_anim = rd32(data + 20);
    a->off_frame = rd32(data + 24);
    a->off_list = rd32(data + 28);
    a->off_elem = rd32(data + 32);
    if (a->off_anim + 4u * a->count > len || a->off_frame + 8u * a->frames > len ||
        a->off_list + 2u * a->list_len > len || a->off_elem + 6u * a->elements > len)
        return -1;
    return 0;
}

uint32_t th_anims_first_frame(const th_anims *a, unsigned anim)
{
    return anim < a->count ? rd16(a->data + a->off_anim + 4 * anim) : 0;
}

uint32_t th_anims_next_frame(const th_anims *a, uint32_t frame)
{
    return frame < a->frames ? rd16(a->data + a->off_frame + 8 * frame + 4) : frame;
}

unsigned th_anims_length(const th_anims *a, unsigned anim)
{
    return anim < a->count ? rd16(a->data + a->off_anim + 4 * anim + 2) : 0;
}

int th_anims_is_multi_frame(const th_anims *a, unsigned anim)
{
    uint32_t f = th_anims_first_frame(a, anim);
    return th_anims_next_frame(a, f) != f;
}

uint32_t th_anims_frame_list(const th_anims *a, uint32_t frame)
{
    return frame < a->frames ? rd32(a->data + a->off_frame + 8 * frame) : a->list_len - 1;
}

uint16_t th_anims_list_entry(const th_anims *a, uint32_t i)
{
    return i < a->list_len ? rd16(a->data + a->off_list + 2 * i) : TH_ANIM_LIST_END;
}

void th_anims_element(const th_anims *a, uint16_t e, th_anim_element *out)
{
    const uint8_t *p = a->data + a->off_elem + 6u * e;
    out->sprite = rd16(p);
    out->x = p[2];
    out->y = p[3];
    out->layer = p[4] >> 4;
    out->flags = p[4] & 0xF;
    out->layer_id = p[5];
    // Como CorsixTH: en la capa 13 o más no vive nada; pasan a la 6.
    if (out->layer >= TH_ANIM_LAYERS)
        out->layer = 6;
}
