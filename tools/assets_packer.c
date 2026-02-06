#include <dirent.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define DS_IMPLEMENTATION
#define DS_NO_PREFIX
#include "ds.h"

da_declare(StringArr, char*);

int main(int argc, char **argv) {
    if(argc != 3) {
        log_error("Usage: ./assets_packer <input_dir> <output_file>\n");
        exit(1);
    }
    String out = {0};
    StringArr assets = {0};
    str_append(&out, "#ifndef ASSETS_H\n");
    str_append(&out, "#define ASSETS_H\n");
    str_append(&out, "#include <stdint.h>\n");
    str_append(&out, "#include <stddef.h>\n\n");
    str_append(&out, "#ifdef ESP32\n");
    str_append(&out, "typedef uint16_t pixel_t;\n");
    str_append(&out, "#else\n");
    str_append(&out, "typedef uint32_t pixel_t;\n");
    str_append(&out, "#endif\n\n");

    DIR *d = opendir(argv[1]);
    struct dirent *dir;
    if (!d) {
        log_error("Error reading directory %s\n", argv[1]);
        exit(1);
    }
    while ((dir = readdir(d)) != NULL) {
        if(dir->d_type != 8) continue;
        if(!ends_with(dir->d_name, ".png")) continue;
        char *name = strdup(dir->d_name);
        char *c=strrchr(name, '.');
        *c = 0;
        da_append(&assets, name);

        str_appendf(&out, "// %s\n", dir->d_name);
        int x, y, ch;
        uint8_t *bitmap = stbi_load("nome", &x, &y, &ch, 0);
        
    }
    closedir(d);

    str_append(&out, "typedef enum {\n");
    str_append(&out, "    NULL_ASSET,\n");
    da_foreach_idx(&assets, i) {
        str_appendf(&out, "    tx_%s,\n", assets.data[i]);
    }
    str_append(&out, "} TextureId;\n\n");

    str_append(&out, "const pixel_t *assets_map[] = {\n");
    str_append(&out, "    NULL,\n");
    da_foreach_idx(&assets, i) {
        str_appendf(&out, "    %s,\n", assets.data[i]);
    }
    str_append(&out, "};\n");

    write_entire_file(argv[2], &out);
    return 0;
}