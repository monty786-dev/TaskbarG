/* Image loading through GDI+ (loaded at run time): png, jpg, bmp, gif and ico files,
 * scaled once and cached as premultiplied 32-bit bitmaps, optionally recoloured. */
#include "app.h"
#include <stdlib.h>
#include <string.h>

typedef struct { UINT32 ver; void *cb; BOOL nobg, noext; } GpInput;
typedef int (WINAPI *Startup_t)(ULONG_PTR *, const GpInput *, void *);
typedef int (WINAPI *FromFile_t)(const wchar_t *, void **);
typedef int (WINAPI *FromHicon_t)(HICON, void **);
typedef int (WINAPI *Dim_t)(void *, UINT *);
typedef int (WINAPI *FromScan_t)(int, int, int, int, void *, void **);
typedef int (WINAPI *Gfx_t)(void *, void **);
typedef int (WINAPI *Draw_t)(void *, void *, int, int, int, int);
typedef int (WINAPI *Mode_t)(void *, int);
typedef int (WINAPI *Free_t)(void *);
typedef int (WINAPI *Clear_t)(void *, UINT32);

static struct { HMODULE dll; ULONG_PTR token; int ok, tried;
    FromFile_t from_file; FromHicon_t from_hicon; Dim_t width, height; FromScan_t from_scan; Gfx_t gfx;
    Draw_t draw; Mode_t smoothing, interp; Free_t del_gfx, dispose; Clear_t clear; } G;

static int gp_load(void) {
    if (G.tried) return G.ok;
    G.tried = 1; G.dll = LoadLibraryW(L"gdiplus.dll"); if (!G.dll) return 0;
    #define FN(f, n) G.f = (void *)GetProcAddress(G.dll, n); if (!G.f) return 0;
    Startup_t st = (Startup_t)GetProcAddress(G.dll, "GdiplusStartup"); if (!st) return 0;
    FN(from_file, "GdipCreateBitmapFromFile") FN(from_hicon, "GdipCreateBitmapFromHICON")
    FN(width, "GdipGetImageWidth") FN(height, "GdipGetImageHeight") FN(from_scan, "GdipCreateBitmapFromScan0")
    FN(gfx, "GdipGetImageGraphicsContext") FN(draw, "GdipDrawImageRectI") FN(smoothing, "GdipSetSmoothingMode")
    FN(interp, "GdipSetInterpolationMode") FN(del_gfx, "GdipDeleteGraphics") FN(dispose, "GdipDisposeImage")
    FN(clear, "GdipGraphicsClear")
    #undef FN
    GpInput in = { 1, NULL, FALSE, FALSE };
    if (st(&G.token, &in, NULL) != 0) return 0;
    G.ok = 1; return 1;
}

#define MAX_IMG 96
typedef struct { wchar_t path[MAX_PATH]; int size; COLORREF tint; int use_tint; HBITMAP bmp; int ok; } Img;
static Img g_img[MAX_IMG]; static int g_nimg;

static int ends_ico(const wchar_t *p) { size_t l = wcslen(p); return l > 4 && _wcsicmp(p + l - 4, L".ico") == 0; }

static Img *img_get(const wchar_t *path, int size, COLORREF tint, int use_tint) {
    for (int i = 0; i < g_nimg; i++)
        if (g_img[i].size == size && g_img[i].use_tint == use_tint && (!use_tint || g_img[i].tint == tint) && !_wcsicmp(g_img[i].path, path))
            return &g_img[i];
    if (g_nimg >= MAX_IMG) img_clear();
    Img *im = &g_img[g_nimg++]; memset(im, 0, sizeof *im);
    lstrcpynW(im->path, path, MAX_PATH); im->size = size; im->tint = tint; im->use_tint = use_tint;
    if (!gp_load() || size < 2) return im;
    void *src = NULL;
    if (ends_ico(path)) {
        HICON ic = (HICON)LoadImageW(NULL, path, IMAGE_ICON, size, size, LR_LOADFROMFILE);
        if (ic) { G.from_hicon(ic, &src); DestroyIcon(ic); }
    }
    if (!src && G.from_file(path, &src) != 0) src = NULL;
    if (!src) return im;
    UINT sw = 0, sh = 0; G.width(src, &sw); G.height(src, &sh);
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = size; bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL; HBITMAP hb = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hb && bits) {
        memset(bits, 0, (size_t)size * size * 4);
        void *dst = NULL, *gfx = NULL;
        if (G.from_scan(size, size, size * 4, 0xE200B /* 32bppPARGB */, bits, &dst) == 0 && dst) {
            if (G.gfx(dst, &gfx) == 0 && gfx) {
                G.interp(gfx, 7); G.smoothing(gfx, 4);
                int dw = size, dh = size;
                if (sw && sh) { if (sw > sh) dh = (int)((long long)size * sh / sw); else dw = (int)((long long)size * sw / sh); }
                G.draw(gfx, src, (size - dw) / 2, (size - dh) / 2, dw, dh);
                G.del_gfx(gfx);
            }
            G.dispose(dst);
        }
        if (use_tint) {                           /* recolour: keep alpha, replace colour */
            unsigned char *p = (unsigned char *)bits;
            for (int i = 0; i < size * size; i++, p += 4) {
                int a = p[3];
                p[0] = (unsigned char)(GetBValue(tint) * a / 255); p[1] = (unsigned char)(GetGValue(tint) * a / 255); p[2] = (unsigned char)(GetRValue(tint) * a / 255);
            }
        }
        im->bmp = hb; im->ok = 1;
    }
    G.dispose(src);
    return im;
}

int img_draw(HDC dc, const wchar_t *path, RECT r, COLORREF tint, int use_tint) {
    if (!path || !path[0]) return 0;
    int s = r.right - r.left < r.bottom - r.top ? r.right - r.left : r.bottom - r.top;
    if (s < 4) return 0;
    Img *im = img_get(path, s, tint, use_tint);
    if (!im->ok || !im->bmp) return 0;
    HDC mem = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(mem, im->bmp);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, (r.left + r.right - s) / 2, (r.top + r.bottom - s) / 2, s, s, mem, 0, 0, s, s, bf);
    SelectObject(mem, old); DeleteDC(mem);
    return 1;
}

void img_clear(void) {
    for (int i = 0; i < g_nimg; i++) if (g_img[i].bmp) DeleteObject(g_img[i].bmp);
    g_nimg = 0;
}
