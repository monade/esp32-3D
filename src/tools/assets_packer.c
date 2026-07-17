#include <dirent.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define DS_IMPLEMENTATION
#define DS_NO_PREFIX
#include "ds.h"

da_declare(StringArr, char*);

void generate_rgb_32(String *buffer, const char *name, uint8_t *bitmap, int x, int y, int ch) {
  str_appendf(buffer, "static const yr_pixel_t %s[] = { \n   ", name);
  for(int i = 0; i < x * y; i++) {
    uint8_t r = bitmap[i * ch + 0];
    uint8_t g = bitmap[i * ch + 1];
    uint8_t b = bitmap[i * ch + 2];
    uint8_t a = ch >= 4 ? bitmap[i * ch + 3] : 255;
    uint32_t pixel = (r << 24) | (g << 16) | (b << 8) | 0xFF;
    if (a == 0) pixel = 0xFF;
    else if (pixel == 0xFF && ch >= 4) pixel = 0x1FF;
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

void generate_mono(String *buffer, const char *name, uint8_t *bitmap, int x, int y, int ch) {
  str_appendf(buffer, "static const yr_pixel_t %s[] = { \n    ", name);
  for(int i = 0; i < x * y; i++) {
    uint8_t r = bitmap[i * ch + 0];
    uint8_t g = bitmap[i * ch + 1];
    uint8_t b = bitmap[i * ch + 2];
    uint8_t a = ch >= 4 ? bitmap[i * ch + 3] : 255;
    uint8_t pixel = (uint8_t)((r * 299 + g * 587 + b * 114) / 1000);
    if (a == 0) pixel = 0;
    else if (pixel == 0 && ch >= 4) pixel = 1;
    str_appendf(buffer, "0x%02X", pixel);
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
  str_appendf(buffer, "static const yr_pixel_t %s[] = { \n    ", name);
  for(int i = 0; i < x * y; i++) {
    uint8_t r = bitmap[i * ch + 0] * 31 / 255;
    uint8_t g = bitmap[i * ch + 1] * 63 / 255;
    uint8_t b = bitmap[i * ch + 2] * 31 / 255;
    uint8_t a = ch >= 4 ? bitmap[i * ch + 3] : 255;
    uint16_t pixel = (r << 11) | (g << 5) | b;
    if (a == 0) pixel = 0;
    else if (pixel == 0 && ch >= 4) pixel = 1 << 5;
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

void generate_asset(String *out, const char *name, uint8_t *bitmap, int x, int y, int ch, bool want_565, bool want_mono) {
    if (want_565) {
        str_append(out, "#ifdef YR_RGB565\n");
        generate_rgb_565(out, name, bitmap, x, y, ch);
    }
    if (want_mono) {
        str_append(out, want_565 ? "#elif defined(YR_L8)\n" : "#ifdef YR_L8\n");
        generate_mono(out, name, bitmap, x, y, ch);
    }
    if (want_565 || want_mono) {
        str_append(out, "#else\n");
    }
    generate_rgb_32(out, name, bitmap, x, y, ch);
    if (want_565 || want_mono) {
        str_append(out, "#endif\n");
    }
}

int main(int argc, char **argv) {
    if(argc < 3) {
        log_error("Usage: ./assets_packer <input_dir> <output_file> [565] [mono]\n");
        exit(1);
    }
    bool want_565 = (argc == 3); // no format args at all => default to 565
    bool want_mono = false;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "565") == 0) want_565 = true;
        else if (strcmp(argv[i], "mono") == 0) want_mono = true;
        else {
            log_error("Unknown format '%s' (expected 565 or mono)\n", argv[i]);
            exit(1);
        }
    }
    String out = {0};
    StringArr assets = {0};
    str_append(&out, "// File generated automatically by assets_packer.c. DO NOT EDIT. \n");
    str_append(&out, "#ifndef YR_ASSETS_H\n");
    str_append(&out, "#define YR_ASSETS_H\n");
    str_append(&out, "#include <stdint.h>\n");
    str_append(&out, "#include <stddef.h>\n\n");
    str_append(&out, "#include <colors.h>\n\n");

    DIR *d = opendir(argv[1]);
    struct dirent *dir;
    if (!d) {
        log_error("Error reading directory %s\n", argv[1]);
        exit(1);
    }
    while ((dir = readdir(d)) != NULL) {
        if(dir->d_type != 8) continue;
        if(!ends_with(dir->d_name, ".png") && !ends_with(dir->d_name, ".jpg")) continue;
        char *name = tmp_strdup(dir->d_name);
        char *c=strrchr(name, '.');
        *c = 0;
        da_append(&assets, name);

        int x, y, ch;
        char cfile[256] = {0};
        strcat(cfile, argv[1]);
        strcat(cfile, "/");
        strcat(cfile, dir->d_name);
        uint8_t *bitmap = stbi_load(cfile, &x, &y, &ch, 0);
        log_info("Packing asset %s (size: %dx%d, channels: %d)", dir->d_name, x, y, ch);
        if (!bitmap) {
          log_error("Error loading image %s\n", cfile);
          exit(1);
        }
        
        str_appendf(&out, "// %s\n", dir->d_name);
        generate_asset(&out, name, bitmap, x, y, ch, want_565, want_mono);
        str_append(&out, "\n");
        STBI_FREE(bitmap);
    }
    closedir(d);

    str_append(&out, "typedef enum {\n");
    str_append(&out, "    NULL_ASSET,\n");
    foreach_idx(&assets, i) {
        str_appendf(&out, "    tx_%s,\n", assets.data[i]);
    }
    str_append(&out, "} TextureId;\n\n");

    str_append(&out, "const yr_pixel_t *assets_map[] = {\n");
    str_append(&out, "    NULL,\n");
    foreach_idx(&assets, i) {
        str_appendf(&out, "    %s,\n", assets.data[i]);
    }
    str_append(&out, "};\n");
    str_append(&out, "#endif //YR_ASSETS_H");

    write_entire_file(argv[2], &out);
    da_free(&out);
    da_free(&assets);
    tmp_free();
    return 0;
}
