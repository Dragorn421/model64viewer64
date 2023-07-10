#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <emscripten/emscripten.h>

#define main _mkmodel_main
#include "../libdragon/tools/mkmodel/mkmodel.c"
#undef main

#include "../inject_offset.c"

int main()
{
    printf("Hello\n");
}

EMSCRIPTEN_KEEPALIVE int wrap_mkmodel_convert(char *path_gltf, char *path_model64)
{
    printf("%s -> %s\n", path_gltf, path_model64);
    int ret = convert(path_gltf, path_model64);
    printf("ret = %d\n", ret);
    return ret;
}

EMSCRIPTEN_KEEPALIVE void inject_model(char *path_rom, char *path_model64, char *path_out_rom)
{
    FILE *f_out_rom = fopen(path_out_rom, "wb");
    // copy rom to out_rom
    FILE *f_rom = fopen(path_rom, "rb");
    int buf_len = 2 * 1024 * 1024;
    void *buf = malloc(buf_len);
    int nread;
    int romsize = 0;
    while ((nread = fread(buf, 1, buf_len, f_rom)) != 0)
    {
        romsize += nread;
        fwrite(buf, 1, nread, f_out_rom);
    }
    fclose(f_rom);

    // offset where to put the model data at
    int model_offset = get_model_offset(romsize);
    // pad the out rom up to model_offset
    int outromsize = romsize;
    memset(buf, 0x42, model_offset - outromsize);
    while (outromsize < model_offset)
    {
        outromsize += fwrite(buf, 1, model_offset - outromsize, f_out_rom);
    }

    // copy model to out_rom
    FILE *f_model = fopen(path_model64, "rb");
    int modelsize = 0;
    while ((nread = fread(buf, 1, buf_len, f_model)) != 0)
    {
        modelsize += nread;
        fwrite(buf, 1, nread, f_out_rom);
    }
    fclose(f_model);

    free(buf);

    write_meta(f_out_rom, model_offset, modelsize);

    fclose(f_out_rom);
}