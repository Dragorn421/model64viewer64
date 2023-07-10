#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/param.h>

#define INJECT_OFFSET_META 0x104000
#define META_SIZE 8

int align16(int v)
{
    return (v + 15) / 16 * 16;
}

int get_model_offset(int rom_size)
{
    return align16(MAX(rom_size, INJECT_OFFSET_META + META_SIZE));
}

void write_meta(FILE *f, int offset, int size)
{
    fseek(f, INJECT_OFFSET_META, SEEK_SET);
    uint8_t meta_buf[META_SIZE] = {
        (offset >> 24) & 0xFF,
        (offset >> 16) & 0xFF,
        (offset >> 8) & 0xFF,
        (offset >> 0) & 0xFF,
        (size >> 24) & 0xFF,
        (size >> 16) & 0xFF,
        (size >> 8) & 0xFF,
        (size >> 0) & 0xFF,
    };
    fwrite(meta_buf, 1, 8, f);
}

void read_meta(void *buf, int *offsetp, int *sizep)
{
    uint8_t meta_buf[META_SIZE];
    memcpy(meta_buf, buf, 8);
    *offsetp = (meta_buf[0] << 24) | (meta_buf[1] << 16) | (meta_buf[2] << 8) | (meta_buf[3] << 0);
    *sizep = (meta_buf[4] << 24) | (meta_buf[5] << 16) | (meta_buf[6] << 8) | (meta_buf[7] << 0);
}
