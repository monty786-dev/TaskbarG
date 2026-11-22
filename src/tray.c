#include "app.h"
#include <string.h>
#include <stdlib.h>

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

/* ---- applications added to the tray: icons that launch (or focus) a program ---- */
#define MAX_TA 32
static RECT g_ta_rect[MAX_TA]; static wchar_t g_ta_path[MAX_TA][MAX_PATH]; static int g_nta;
static int ta_count(void) { JNode *a = cfg_node("tray_apps"); int n = a ? json_count(a) : 0; return n > MAX_TA ? MAX_TA : n; }

int tray_width(void) { return (g_n + ta_count()) * px(cfg_int("tray_cell")); }

void tray_draw(HDC dc, RECT area, int vertical) {
    int cell = px(cfg_int("tray_cell")), sz = cell * 2 / 3, pos = vertical ? area.top : area.left;
    for (int i = 0; i < g_n; i++) {
        TrayIcon *t = &g_icons[i];
        if (vertical) { t->rect.left = area.left; t->rect.right = area.right; t->rect.top = pos; t->rect.bottom = pos + cell; }
        else { t->rect.left = pos; t->rect.right = pos + cell; t->rect.top = area.top; t->rect.bottom = area.bottom; }
        if (t->has && t->bmp) {
            HDC src = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(src, t->bmp);
            int cx = (t->rect.left + t->rect.right) / 2, cy = (t->rect.top + t->rect.bottom) / 2;
            TransparentBlt(dc, cx - sz / 2, cy - sz / 2, sz, sz, src, 0, 0, t->w, t->hh, t->key);
            SelectObject(src, old); DeleteDC(src);
        }
        pos += cell;
    }
    JNode *a = cfg_node("tray_apps"); g_nta = 0;
    for (JNode *c = a ? a->child : NULL; c && g_nta < MAX_TA; c = c->next) {
        if (!c->s) continue; w_from_utf8_buf(c->s, g_ta_path[g_nta], MAX_PATH);
        RECT r = vertical ? (RECT){ area.left, pos, area.right, pos + cell } : (RECT){ pos, area.top, pos + cell, area.bottom }; g_ta_rect[g_nta] = r;
        int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2; g_icon_a(dc, icon_for_file(g_ta_path[g_nta]), cx - sz / 2, cy - sz / 2, sz, cfg_int("bar_icon_alpha"));
        pos += cell; g_nta++;
    }
}

static void ta_remove(int idx) {
    JNode *a = cfg_node("tray_apps"); if (!a) return; JNode *c = a->child, *prev = NULL; int i = 0;
    while (c) { JNode *nx = c->next; if (i == idx) { if (prev) prev->next = nx; else a->child = nx; c->next = NULL; json_free(c); break; } prev = c; c = nx; i++; }
    app_save(); bar_invalidate();
}
void tray_add_app(const wchar_t *path) {
    char *u = utf8_from_w(path); JNode *a = cfg_node("tray_apps");
    if (a && a->t == J_ARR && !cfg_list_has("tray_apps", u)) { json_push(a, json_str(u)); app_save(); bar_invalidate(); }
    free(u);
}

int tray_click(int x, int y, int button) {
    for (int i = 0; i < g_nta; i++) if (in_rect(&g_ta_rect[i], x, y)) {
        if (button == 1) { const wchar_t *lab[] = { L"Open", L"Remove from tray" }; int id[] = { 1, 2 }, fl[] = { 0, 0 }; int cmd = app_popup(g_bar_hwnd, lab, id, fl, 2, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR);
            if (cmd == 1) app_run(g_ta_path[i], NULL, SW_SHOWNORMAL); else if (cmd == 2) ta_remove(i); }
        else if (button == 0) app_run(g_ta_path[i], NULL, SW_SHOWNORMAL);
        return 1;
    }
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
