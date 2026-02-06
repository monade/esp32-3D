#include <dirent.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define DS_IMPLEMENTATION
#define DS_NO_PREFIX
#include "ds.h"

da_declare(StringArr, char*);

void generate_rgb_32(String *buffer, const char *name, uint8_t *bitmap, int x, int y, int ch) {
  str_appendf(buffer, "static const pixel_t %s[] = { \n   ", name);
  for(int i = 0; i < x * y; i++) {
    uint8_t r = bitmap[i * ch + 0];
    uint8_t g = bitmap[i * ch + 1];
    uint8_t b = bitmap[i * ch + 2];
    uint32_t pixel = (r << 24) | (g << 16) | (b << 8) | 0xFF;
    str_appendf(buffer, "0x%08X", pixel);
    if (i % 8 == 7) {
      if (i != (x * y) - 1) {
        str_appendf(buffer, ",\n    ");
      } else {
        str_appendf(buffer, ", ");
      }
    } else {
      str_appendf(buffer, ", ");
    }
  }
  str_appendf(buffer, "\n};\n");
}

void generate_rgb_565(String *buffer, const char *name, uint8_t *bitmap, int x, int y, int ch) {
  str_appendf(buffer, "static const pixel_t %s[] = { \n    ", name);
  for(int i = 0; i < x * y; i++) {
    uint8_t r = bitmap[i * ch + 0] * 31 / 255;
    uint8_t g = bitmap[i * ch + 1] * 63 / 255;
    uint8_t b = bitmap[i * ch + 2] * 31 / 255;
    uint16_t pixel = (r << 11) | (g << 5) | b;
    str_appendf(buffer, "0x%04X", pixel);
    if (i % 8 == 7) {
      if (i != (x * y) - 1) {
        str_appendf(buffer, ",\n    ");
      } else {
        str_appendf(buffer, ", ");
      }
    } else {
      str_appendf(buffer, ", ");
    }
  }
  str_appendf(buffer, "\n};\n");
}

int main(int argc, char **argv) {
    if(argc != 3) {
        log_error("Usage: ./assets_packer <input_dir> <output_file>\n");
        exit(1);
    }
    String out = {0};
    StringArr assets = {0};
    str_append(&out, "// File generated automatically by assets_packer.c. DO NOT EDIT. \n");
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
        if(!ends_with(dir->d_name, ".png") && !ends_with(dir->d_name, ".jpg")) continue;
        char *name = strdup(dir->d_name);
        char *c=strrchr(name, '.');
        *c = 0;
        da_append(&assets, name);

        int x, y, ch;
        char cfile[256] = {0};
        strcat(cfile, argv[1]);
        strcat(cfile, "/");
        strcat(cfile, dir->d_name);
        uint8_t *bitmap = stbi_load(cfile, &x, &y, &ch, 0);
        printf("cfile content: %s\n", cfile);
        printf("Bitmap %p, x: %d, y: %d, ch: %d\n", bitmap, x, y, ch);
        if (!bitmap) {
            log_error("Error loading image %s\n", cfile);
            exit(1);
        }

        str_appendf(&out, "// %s\n", dir->d_name);
        str_append(&out, "#ifdef ESP32\n");
        generate_rgb_565(&out, name, bitmap, x, y, ch);
        str_append(&out, "#else\n");
        generate_rgb_32(&out, name, bitmap, x, y, ch);
        str_append(&out, "#endif\n\n");
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
    str_append(&out, "#endif //ASSETS_H");

    write_entire_file(argv[2], &out);
    return 0;
}
