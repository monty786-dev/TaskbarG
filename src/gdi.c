#include "app.h"
#include <string.h>

HFONT g_font, g_font_big, g_font_small;

static HFONT make_font(const wchar_t *name, int size, int weight) {
    return CreateFontW(-size, 0, 0, 0, weight, 0, 0, 0, 1, 0, 0, 5, 0, name);
}

void apply_fonts(void) {
    if (g_font) DeleteObject(g_font);
    if (g_font_big) DeleteObject(g_font_big);
    if (g_font_small) DeleteObject(g_font_small);
    wchar_t name[64]; w_from_utf8_buf(cfg_str("font"), name, 64);
    if (!name[0]) lstrcpyW(name, L"Segoe UI");
    int size = px(cfg_int("font_size"));
    g_font = make_font(name, size, 400);
    g_font_big = make_font(name, size + 2, 600);
    g_font_small = make_font(name, size - 1 < 9 ? 9 : size - 1, 400);
}

int in_rect(const RECT *r, int x, int y) { return x >= r->left && x < r->right && y >= r->top && y < r->bottom; }

void g_fill(HDC dc, RECT r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b);
}
void g_frame(HDC dc, RECT r, COLORREF c, int t) {
    RECT a = { r.left, r.top, r.right, r.top + t }; g_fill(dc, a, c);
    RECT b = { r.left, r.bottom - t, r.right, r.bottom }; g_fill(dc, b, c);
    RECT l = { r.left, r.top, r.left + t, r.bottom }; g_fill(dc, l, c);
    RECT rr = { r.right - t, r.top, r.right, r.bottom }; g_fill(dc, rr, c);
}
void g_alpha(HDC dc, RECT r, COLORREF c, int alpha) {
    if (r.right <= r.left || r.bottom <= r.top) return;
    HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, 1, 1);
    HGDIOBJ old = SelectObject(mem, bmp); SetPixel(mem, 0, 0, c);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(alpha & 255), 0 };
    AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, mem, 0, 0, 1, 1, bf);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
}
void g_vgrad(HDC dc, RECT r, COLORREF c1, COLORREF c2, int step) {
    int h = r.bottom - r.top; if (h <= 0) return;
    if (step < 1) step = 1;
    for (int y = r.top; y < r.bottom; y += step) {
        double f = (double)(y - r.top) / (double)(h - 1 > 1 ? h - 1 : 1); if (f > 1) f = 1;
        RECT s = { r.left, y, r.right, y + step > r.bottom ? r.bottom : y + step };
        g_fill(dc, s, c_mix(c1, c2, f));
    }
}
void g_text(HDC dc, const wchar_t *s, RECT r, UINT flags, COLORREF c) {
    SetTextColor(dc, c); DrawTextW(dc, s, -1, &r, flags);
}

void apply_glass(HWND h, int on, int pct) {
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if (on) {
        SetWindowLongPtrW(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        if (pct < 30) pct = 30; if (pct > 100) pct = 100;
        SetLayeredWindowAttributes(h, 0, (BYTE)(255 * pct / 100), LWA_ALPHA);
    } else if (ex & WS_EX_LAYERED) {
        SetWindowLongPtrW(h, GWL_EXSTYLE, ex & ~(LONG_PTR)WS_EX_LAYERED);
    }
}

/* ------------------------------------------------- cached glass backdrop */
typedef struct {
    HBITMAP bmp; int w, h, dark, gloss; COLORREF base; char theme[16];
} GlassEnt;
static GlassEnt g_cache[2];

static void render_glass(HDC mem, int w, int h, COLORREF base, int slot) {
    int dark = app_is_dark();
    int gloss = cfg_int("glass_gloss"); if (gloss < 0) gloss = 0; if (gloss > 100) gloss = 100;
    const char *theme = cfg_str("glass_theme");
    RECT all = { 0, 0, w, h };
    g_vgrad(mem, all, c_mix(base, RGB(255, 255, 255), dark ? 0.12 : 0.0), base, h / 40 > 1 ? h / 40 : 1);
    int sheen = h / 2 > 2 ? h / 2 : 2, steps = sheen < 22 ? sheen : 22;
    int base_alpha = (dark ? 50 : 75) * gloss / 100;
    for (int i = 0; i < steps; i++) {
        int y0 = sheen * i / steps, y1 = sheen * (i + 1) / steps;
        int alpha = (int)(base_alpha * (1.0 - (double)i / steps));
        RECT r = { 0, y0, w, y1 > y0 ? y1 : y0 + 1 };
        g_alpha(mem, r, RGB(255, 255, 255), alpha);
    }
    int hair = dark ? 105 : 170;
    if (!strcmp(theme, "macos")) {
        RECT r = { 0, 0, w, h / 24 > 2 ? h / 24 : 2 }; g_alpha(mem, r, RGB(255, 255, 255), hair / 2);
    } else if (!strcmp(theme, "arch")) {
        RECT r = { 0, 0, w, h / 40 > 2 ? h / 40 : 2 }; g_alpha(mem, r, RGB(255, 255, 255), hair / 2);
        RECT b = { 0, h - 1, w, h }; g_alpha(mem, b, RGB(0, 0, 0), 90);
    } else {
        RECT t = { 0, 0, w, 1 }; g_alpha(mem, t, RGB(255, 255, 255), hair);
        RECT b = { 0, h - 1, w, h }; g_alpha(mem, b, RGB(0, 0, 0), 70);
    }
    if (slot == 1) {
        RECT l = { 0, 0, 1, h }; g_alpha(mem, l, RGB(255, 255, 255), 60);
        RECT r = { w - 1, 0, w, h }; g_alpha(mem, r, RGB(255, 255, 255), 60);
    }
}

void g_glass_blit(HDC dc, int slot, int w, int h, COLORREF base) {
    if (w < 1) w = 1; if (h < 1) h = 1;
    GlassEnt *e = &g_cache[slot];
    int dark = app_is_dark(), gloss = cfg_int("glass_gloss");
    if (!e->bmp || e->w != w || e->h != h || e->dark != dark || e->gloss != gloss ||
        e->base != base || strncmp(e->theme, cfg_str("glass_theme"), 15) != 0) {
        if (e->bmp) DeleteObject(e->bmp);
        HDC screen = GetDC(NULL); HDC mem = CreateCompatibleDC(screen);
        e->bmp = CreateCompatibleBitmap(screen, w, h);
        HGDIOBJ old = SelectObject(mem, e->bmp);
        render_glass(mem, w, h, base, slot);
        SelectObject(mem, old); DeleteDC(mem); ReleaseDC(NULL, screen);
        e->w = w; e->h = h; e->dark = dark; e->gloss = gloss; e->base = base;
        lstrcpynA(e->theme, cfg_str("glass_theme"), 16);
    }
    HDC src = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(src, e->bmp);
    BitBlt(dc, 0, 0, w, h, src, 0, 0, SRCCOPY);
    SelectObject(src, old); DeleteDC(src);
}

void g_glass_clear(void) {
    for (int i = 0; i < 2; i++) { if (g_cache[i].bmp) DeleteObject(g_cache[i].bmp); memset(&g_cache[i], 0, sizeof g_cache[i]); }
}

/* Draw an icon at reduced opacity: blend (background + icon) over the background. */
void g_icon_a(HDC dc, HICON ic, int x, int y, int size, int alpha_pct) {
    if (!ic) return;
    if (alpha_pct >= 100) { DrawIconEx(dc, x, y, ic, size, size, 0, NULL, DI_NORMAL); return; }
    if (alpha_pct <= 0) return;
    HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, size, size); HGDIOBJ old = SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, size, size, dc, x, y, SRCCOPY); DrawIconEx(mem, 0, 0, ic, size, size, 0, NULL, DI_NORMAL);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(255 * alpha_pct / 100), 0 };
    AlphaBlend(dc, x, y, size, size, mem, 0, 0, size, size, bf);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
}
COLORREF g_mixa(COLORREF bg, COLORREF fg, int alpha_pct) { if (alpha_pct >= 100) return fg; if (alpha_pct < 0) alpha_pct = 0; return c_mix(bg, fg, alpha_pct / 100.0); }
