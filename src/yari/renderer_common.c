#include <math.h>
#include "renderer.h"

void yr_draw_texture(
    int x,
    int y,
    int width,
    int height,
    const yr_pixel_t *texture,
    int texture_width,
    int texture_height,
    bool skip_empty
) {
    if (width <= 0 || height <= 0 ||
        texture_width <= 0 || texture_height <= 0 ||
        !texture) {
        return;
    }

    for (int dst_row = 0; dst_row < height; dst_row++) {
        int src_row = dst_row * texture_height / height;

        for (int dst_col = 0; dst_col < width; dst_col++) {
            int src_col = dst_col * texture_width / width;

            yr_pixel_t pixel = texture[src_row * texture_width + src_col];

            if (!skip_empty || pixel != YR_EMPTY_PIXEL) {
                yr_draw_rectangle(x + dst_col, y + dst_row, 1, 1, pixel);
            }
        }
    }
}

/**
 *
 * @param text null-terminated string to draw
 * @param x x position of the top-left corner
 * @param y y position of the top-left corner
 * @param font font to use for rendering
 * @param c color to use for rendering
 */
void yr_draw_text(
    const char* text,
    const int x,
    const int y,
    const yr_font_t* font,
    const yr_pixel_t c
) {
    int cursor_x = x;
    for (const char *p = text; *p; p++) {
        char ch = *p;
        if (ch < 32) continue;

        yr_glyph_t g = font->glyphs[(unsigned char)ch - 32];
        int dst_x = cursor_x + (int)g.xoff;
        int dst_y = y + (int)g.yoff;

        for (int gy = g.y0; gy < g.y1; gy++) {
            for (int gx = g.x0; gx < g.x1; gx++) {
                int i = gy * font->atlas_w + gx;
                if ((font->atlas[i >> 3] >> (i & 7)) & 1)
                    yr_draw_rectangle(dst_x + (gx - g.x0), dst_y + (gy - g.y0), 1, 1, c);
            }
        }
        cursor_x += (int)g.xadvance;
    }
}

void yr_draw_line(int x0, int y0, int x1, int y1, int thickness, yr_pixel_t color) {
    int size = thickness;
    int sm = size/2;
    int xv = x1-x0;
    int yv = y1-y0;
    if(x0 == x1) {
        if(yv>0) {
            yr_draw_rectangle(x0-sm, y0, size, yv, color);
        } else {
            yr_draw_rectangle(x1-sm, y1, size, -yv, color);
        }
        return;
    }
    if(y0 == y1) {
        if(xv>0) {
            yr_draw_rectangle(x0, y0-sm, xv, size, color);
        } else {
            yr_draw_rectangle(x1, y1-sm, -xv, size, color);
        }
        return;
    }
    float m = (float)yv/xv;
    int xs, ys, xe;
    if (xv>0) {
        xs = x0;
        ys = y0;
        xe = x1;
    } else {
        xs = x1;
        ys = y1;
        xe = x0;
    }
    for(int x=xs; x<xe; x++) {
        int y = m*(x-xs);
        yr_draw_rectangle(x-sm, y-sm+ys, size, size, color);
    }
}

void yr_draw_rectangle_line(int x, int y, int width, int height, int thickness, yr_pixel_t color) {
    yr_draw_line(x, y, x+width, y, thickness, color);
    yr_draw_line(x, y, x, y+height, thickness, color);
    yr_draw_line(x+width, y, x+width, y+height, thickness, color);
    yr_draw_line(x, y+height, x+width, y+height, thickness, color);
}

void yr_draw_circle(int x, int y, int radius, yr_pixel_t color) {
    int r2 = radius*radius;
    for(int i=-radius; i<radius; i++) {
        // x2+y2 = r2
        int y0 = sqrtf(r2 - i*i);
        yr_draw_rectangle(x+i,y-y0, 1, y0*2, color);
    }
}

void yr_draw_circle_line(int x, int y, int radius, int thickness, yr_pixel_t color) {
    int sm = thickness/2;
    int r2 = radius*radius;
    int oy=0;
    for(int i=-radius; i<=radius; i++) {
        int y0 = sqrtf(r2 - i*i);
        int dy = y0 - oy;
        if (dy>=0) {
            yr_draw_rectangle(x+i-sm, y-y0-sm, thickness, dy + thickness, color);
            yr_draw_rectangle(x+i-sm, y+oy-sm, thickness, dy + thickness, color);
        } else {
            yr_draw_rectangle(x+i-sm, y-oy-sm, thickness, -dy + thickness, color);
            yr_draw_rectangle(x+i-sm, y+y0-sm, thickness, -dy + thickness, color);
        }
        oy = y0;
    }
}

#define YR_SAME_SIGN(x,y) ((((x)>=0) && ((y)>=0)) || (((x)<0) && ((y)<0)))
static inline bool yr_inside_triangle(int x1, int y1, int x2, int y2, int x3, int y3, int xp, int yp)
{
    int det = ((x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3));
    int v1  = ((y2 - y3)*(xp - x3) + (x3 - x2)*(yp - y3));
    int v2  = ((y3 - y1)*(xp - x3) + (x1 - x3)*(yp - y3));
    int v3 = det - v1 - v2;
    return (
               (YR_SAME_SIGN(v1, det) || v1 == 0) &&
               (YR_SAME_SIGN(v2, det) || v2 == 0) &&
               (YR_SAME_SIGN(v3, det) || v3 == 0)
           );
}

static inline void yr_normalize_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int *lx, int *hx, int *ly, int *hy)
{
    *lx = x0;
    *hx = x0;
    if (*lx > x1) *lx = x1;
    if (*lx > x2) *lx = x2;
    if (*hx < x1) *hx = x1;
    if (*hx < x2) *hx = x2;
    if (*lx < 0) *lx = 0;

    *ly = y0;
    *hy = y0;
    if (*ly > y1) *ly = y1;
    if (*ly > y2) *ly = y2;
    if (*hy < y1) *hy = y1;
    if (*hy < y2) *hy = y2;
    if (*ly < 0) *ly = 0;
}

void yr_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, yr_pixel_t color) {
    int lx, hx, ly, hy;
    yr_normalize_triangle(x0, y0, x1, y1, x2, y2, &lx, &hx, &ly, &hy);

    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            if (yr_inside_triangle(x0, y0, x1, y1, x2, y2, x, y)) {
                yr_draw_pixel(x, y, color);
            }
        }
    }
}

void yr_draw_triangle_line(int x0, int y0, int x1, int y1, int x2, int y2, int thickness, yr_pixel_t color) {
    yr_draw_line(x0, y0, x1, y1, thickness, color);
    yr_draw_line(x1, y1, x2, y2, thickness, color);
    yr_draw_line(x2, y2, x0, y0, thickness, color);
}
