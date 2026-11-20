#include "app.h"
#include <string.h>

#define MAX_TRAY 24
typedef struct { HWND h; int w, hh; HBITMAP bmp; COLORREF key; int has; RECT rect; } TrayIcon;
static TrayIcon g_icons[MAX_TRAY]; static int g_n;
static DWORD g_last;

typedef struct { HWND h; int w, hh; } Found;
typedef struct { Found f[MAX_TRAY]; int n; } FoundList;

static BOOL CALLBACK enum_cb(HWND hwnd, LPARAM lp) {
    FoundList *l = (FoundList *)lp; RECT rc; wchar_t cls[128];
    if (l->n >= MAX_TRAY || !GetWindowRect(hwnd, &rc)) return TRUE;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    GetClassNameW(hwnd, cls, 128); _wcslwr(cls);
    if (w >= 8 && w <= 48 && h >= 8 && h <= 48 && !wcsstr(cls, L"tooltip") && wcscmp(cls, L"static") != 0) {
        l->f[l->n].h = hwnd; l->f[l->n].w = w; l->f[l->n].hh = h; l->n++;
    }
    return TRUE;
}

static void grab(TrayIcon *t) {
    HDC dc = GetDC(NULL), mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, t->w, t->hh);
    HGDIOBJ old = SelectObject(mem, bmp);
    BOOL ok = PrintWindow(t->h, mem, 1) || PrintWindow(t->h, mem, 0);
    COLORREF key = GetPixel(mem, 0, 0);
    SelectObject(mem, old); DeleteDC(mem); ReleaseDC(NULL, dc);
    if (ok) { if (t->bmp) DeleteObject(t->bmp); t->bmp = bmp; t->key = key; t->has = 1; }
    else DeleteObject(bmp);
}

int tray_refresh(void) {
    DWORD now = GetTickCount();
    if (now - g_last < 1500) return 0;
    g_last = now;
    if (!cfg_bool("tray_icons")) { int changed = g_n > 0; tray_close(); return changed; }
    FoundList l; l.n = 0;
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    if (tray) EnumChildWindows(tray, enum_cb, (LPARAM)&l);
    int changed = l.n != g_n;
    for (int i = 0; i < l.n && !changed; i++) if (g_icons[i].h != l.f[i].h) changed = 1;
    /* keep bitmaps for icons that still exist */
    TrayIcon next[MAX_TRAY]; memset(next, 0, sizeof next);
    for (int i = 0; i < l.n; i++) {
        next[i].h = l.f[i].h; next[i].w = l.f[i].w; next[i].hh = l.f[i].hh;
        for (int j = 0; j < g_n; j++) if (g_icons[j].h == l.f[i].h) { next[i].bmp = g_icons[j].bmp; next[i].key = g_icons[j].key; next[i].has = g_icons[j].has; g_icons[j].bmp = NULL; }
    }
    for (int j = 0; j < g_n; j++) if (g_icons[j].bmp) DeleteObject(g_icons[j].bmp);
    memcpy(g_icons, next, sizeof next); g_n = l.n;
    for (int i = 0; i < g_n; i++) grab(&g_icons[i]);
    return changed;
}

int tray_width(void) { return g_n * px(24); }

void tray_draw(HDC dc, RECT area) {
    int sz = px(16), cell = px(24), x = area.left;
    for (int i = 0; i < g_n; i++) {
        TrayIcon *t = &g_icons[i];
        t->rect.left = x; t->rect.top = area.top; t->rect.right = x + cell; t->rect.bottom = area.bottom;
        if (t->has && t->bmp) {
            HDC src = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(src, t->bmp);
            TransparentBlt(dc, x + (cell - sz) / 2, (area.top + area.bottom) / 2 - sz / 2, sz, sz, src, 0, 0, t->w, t->hh, t->key);
            SelectObject(src, old); DeleteDC(src);
        }
        x += cell;
    }
}

int tray_click(int x, int y, int button) {
    for (int i = 0; i < g_n; i++) {
        TrayIcon *t = &g_icons[i];
        if (!in_rect(&t->rect, x, y)) continue;
        LPARAM pos = (LPARAM)(((t->hh / 2) << 16) | (t->w / 2));
        if (button == 1) { PostMessageW(t->h, WM_RBUTTONDOWN, 2, pos); PostMessageW(t->h, WM_RBUTTONUP, 0, pos); }
        else if (button == 2) { PostMessageW(t->h, WM_LBUTTONDBLCLK, 1, pos); PostMessageW(t->h, WM_LBUTTONUP, 0, pos); }
        else { PostMessageW(t->h, WM_LBUTTONDOWN, 1, pos); PostMessageW(t->h, WM_LBUTTONUP, 0, pos); }
        return 1;
    }
    return 0;
}

void tray_close(void) {
    for (int i = 0; i < g_n; i++) if (g_icons[i].bmp) DeleteObject(g_icons[i].bmp);
    memset(g_icons, 0, sizeof g_icons); g_n = 0;
}
