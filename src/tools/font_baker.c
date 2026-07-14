#include <stdlib.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define DS_IMPLEMENTATION
#define DS_NO_PREFIX
#include "ds.h"

// ASCII range baked into every atlas.
#define FIRST_CHAR  32
#define NUM_CHARS   96   // 32–127

// Per-size config: pixel height and atlas dimensions.
// Sizes are chosen so all ASCII glyphs of press_start_2p (and similar
// pixel fonts) fit without overflow. Atlas area is stored in flash (const),
// so the extra bytes are not a concern on ESP32.
static const float PIXEL_HEIGHTS[4] = {10.0f, 16.0f, 28.0f, 48.0f};
static const char* SIZE_NAMES[4] = {"sm", "md", "lg", "xl"};
static const int ATLAS_WS[4] = {100, 260, 260, 500};
static const int ATLAS_HS[4] = {100, 250, 250, 400};

// atlas is 1-bit packed (LSB-first): pixel i → byte i/8, bit i%8
static void emit_atlas(String *out, const char *name, const char *size,
                       const uint8_t *atlas, int w, int h) {
    int nbytes = (w * h + 7) / 8;
    str_appendf(out, "static const uint8_t _%s_%s_atlas[%d] = {\n    ",
                name, size, nbytes);
    for (int i = 0; i < nbytes; i++) {
        str_appendf(out, "0x%02X", atlas[i]);
        if (i < nbytes - 1)
            str_appendf(out, i % 16 == 15 ? ",\n    " : ", ");
    }
    str_appendf(out, "\n};\n");
}

static void emit_glyphs(String *out, const char *name, const char *size,
                        const stbtt_bakedchar *chars) {
    str_appendf(out, "static const yr_glyph_t _%s_%s_glyphs[%d] = {\n",
                name, size, NUM_CHARS);
    for (int i = 0; i < NUM_CHARS; i++) {
        str_appendf(out, "    {%d,%d,%d,%d,%.3ff,%.3ff,%.3ff},\n",
            chars[i].x0, chars[i].y0, chars[i].x1, chars[i].y1,
            chars[i].xoff, chars[i].yoff, chars[i].xadvance);
    }
    str_appendf(out, "};\n");
}

static void emit_font_t(String *out, const char *name, const char *size,
                        int w, int h) {
    str_appendf(out,
        "static const yr_font_t %s_%s = { _%s_%s_atlas, _%s_%s_glyphs, %d, %d };\n\n",
        name, size, name, size, name, size, w, h);
}

static void bake_font(String *out, const char *name, unsigned char *ttf_data) {
    for (int s = 0; s < 4; s++) {
        int w = ATLAS_WS[s], h = ATLAS_HS[s];
        int npixels = w * h;
        int nbytes = (npixels + 7) / 8;

        uint8_t* gray = malloc((size_t)npixels);
        uint8_t* packed = calloc((size_t)nbytes, 1);
        stbtt_bakedchar chars[NUM_CHARS];

        int ret = stbtt_BakeFontBitmap(ttf_data, 0, PIXEL_HEIGHTS[s],
                                       gray, w, h,
                                       FIRST_CHAR, NUM_CHARS, chars);
        if (ret < 0)
            log_warn("'%s' size '%s': atlas too small, %d chars didn't fit\n",
                     name, SIZE_NAMES[s], -ret);

        // threshold 8-bit → 1-bit (LSB-first: pixel i → bit i%8 of byte i/8)
        for (int i = 0; i < npixels; i++) {
            if (gray[i] > 127)
                packed[i >> 3] |= (uint8_t)(1 << (i & 7));
        }
        free(gray);

        log_info("  %s: atlas %dx%d → %d bytes",
                 SIZE_NAMES[s], w, h, nbytes);

        emit_atlas(out, name, SIZE_NAMES[s], packed, w, h);
        emit_glyphs(out, name, SIZE_NAMES[s], chars);
        emit_font_t(out, name, SIZE_NAMES[s], w, h);
        free(packed);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        log_error("Usage: ./font_baker <input_dir> <output_file>\n");
        log_error("  Bakes every *.ttf in <input_dir> into a C header.\n");
        exit(1);
    }

    String out = {0};
    char *first_font = NULL;

    str_append(&out, "// File generated automatically by font_baker.c. DO NOT EDIT.\n");
    str_append(&out, "#ifndef YR_FONTS_H\n");
    str_append(&out, "#define YR_FONTS_H\n");
    str_append(&out, "#include <stdint.h>\n");
    str_append(&out, "#include <renderer.h>\n\n");

    DIR *d = opendir(argv[1]);
    if (!d) {
        log_error("Cannot open directory: %s\n", argv[1]);
        exit(1);
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type != 8) continue;
        if (!ends_with(dir->d_name, ".ttf")) continue;

        char path[512] = {0};
        strcat(path, argv[1]);
        strcat(path, "/");
        strcat(path, dir->d_name);

        String ttf_buf = {0};
        if (!read_entire_file(path, &ttf_buf)) {
            log_error("Cannot read font: %s\n", path);
            exit(1);
        }
        log_info("Baking %s (%.1f KB)", dir->d_name, ttf_buf.length / 1024.0f);

        char *name = tmp_strdup(dir->d_name);
        *strrchr(name, '.') = '\0';

        str_appendf(&out, "// %s\n", dir->d_name);
        bake_font(&out, name, (unsigned char *)ttf_buf.data);
        da_free(&ttf_buf);

        if (!first_font)
            first_font = tmp_strdup(name);
    }
    closedir(d);

    // fonts[4]: maps font_size_t → yr_font_t* for the first (primary) font found.
    // draw_text() uses this to resolve FONT_SM / FONT_MD / FONT_LG / FONT_XL at runtime.
    if (first_font) {
        str_appendf(&out,
                    "static const yr_font_t *fonts[4] = {\n"
                    "    &%s_sm,\n"
                    "    &%s_md,\n"
                    "    &%s_lg,\n"
                    "    &%s_xl\n"
                    "};\n",
                    first_font, first_font, first_font, first_font);
    }

    str_append(&out, "#endif // YR_FONTS_H");

    write_entire_file(argv[2], &out);
    da_free(&out);
    tmp_free();
    return 0;
}
