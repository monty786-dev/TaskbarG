/* Full game mode: a console-style launcher (PlayStation 5, Steam Big Picture, Nintendo Switch,
 * Xbox, retro list). Mouse, keyboard and XInput controller. Games, icons, backgrounds and
 * descriptions are edited in Settings > Game mode. */
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLASS_NAME L"WinShellGame"
#define MAX_G 64
static HWND g_w; static int g_open, g_sel, g_hover = -1, g_nbtn_hot = -1; static double g_pos; static HANDLE g_proc;
static RECT g_cell[MAX_G], g_btn[3]; static HFONT g_f1, g_f2, g_f3; static int g_fh;
typedef struct { WORD wButtons; BYTE bLT, bRT; SHORT lx, ly, rx, ry; } XPad;
typedef struct { DWORD packet; XPad pad; } XState;
typedef DWORD (WINAPI *XGet_t)(DWORD, XState *);
static XGet_t g_xget; static int g_xtried; static WORD g_xprev; static DWORD g_xlast;

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static JNode *games(void) { JNode *a = cfg_node("games"); return a && a->t == J_ARR ? a : NULL; }
static int ng(void) { JNode *a = games(); return a ? json_count(a) : 0; }
static JNode *game_at(int i) { JNode *a = games(); if (!a) return NULL; for (JNode *c = a->child; c; c = c->next, i--) if (i == 0) return c; return NULL; }
static const char *gs(JNode *g, const char *k) { JNode *v = g ? json_get(g, k) : NULL; return v && v->t == J_STR && v->s ? v->s : ""; }
int game_is_open(void) { return g_open; }

/* ------------------------------------------------------------------ helpers */
static void fonts(int H) {
    if (g_fh == H && g_f1) return; if (g_f1) DeleteObject(g_f1); if (g_f2) DeleteObject(g_f2); if (g_f3) DeleteObject(g_f3);
    wchar_t fn[64]; w_from_utf8_buf(cfg_str("font"), fn, 64); if (!fn[0]) lstrcpyW(fn, L"Segoe UI");
    g_f1 = CreateFontW(-(H * 55 / 1000), 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 5, 0, fn); g_f2 = CreateFontW(-(H * 26 / 1000), 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, fn); g_f3 = CreateFontW(-(H * 20 / 1000), 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, fn); g_fh = H;
}
static COLORREF palette(int i) { static const COLORREF P[] = { RGB(0,120,215), RGB(16,137,62), RGB(218,59,1), RGB(142,140,216), RGB(0,153,188), RGB(194,57,179), RGB(209,52,56), RGB(0,178,148) }; return P[i % 8]; }
static void cover(HDC dc, int i, RECT r, int round) {
    JNode *g = game_at(i); COLORREF c = gs(g, "color")[0] ? c_hex(gs(g, "color"), palette(i)) : palette(i); g_fill(dc, r, c);
    wchar_t icon[MAX_PATH], path[MAX_PATH]; w_from_utf8_buf(gs(g, "icon"), icon, MAX_PATH); w_from_utf8_buf(gs(g, "path"), path, MAX_PATH);
    int w = r.right - r.left, h = r.bottom - r.top, s = (w < h ? w : h) * 60 / 100; RECT ir = R((r.left + r.right) / 2 - s / 2, (r.top + r.bottom) / 2 - s / 2, (r.left + r.right) / 2 + s / 2, (r.top + r.bottom) / 2 + s / 2);
    if (icon[0] && img_draw(dc, icon, R(r.left, r.top, r.right, r.bottom), 0, 0)) { (void)round; return; }
    HICON ic = icon_for_file(path); DrawIconEx(dc, ir.left, ir.top, ic, s, s, 0, NULL, DI_NORMAL);
}
static void wtext(HDC dc, HFONT f, const char *u, RECT r, UINT fl, COLORREF c) { wchar_t w[512]; w_from_utf8_buf(u, w, 512); HGDIOBJ o = SelectObject(dc, f); g_text(dc, w, r, fl, c); SelectObject(dc, o); }
static void pill(HDC dc, RECT r, COLORREF fill, COLORREF line, int round) {
    HBRUSH b = CreateSolidBrush(fill); HPEN p = CreatePen(PS_SOLID, 1, line); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p); RoundRect(dc, r.left, r.top, r.right, r.bottom, round, round); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}
static void buttons(HDC dc, int W, int H, COLORREF fill, COLORREF txt, COLORREF line, int round, const wchar_t *labels[3]) {
    int bw = W * 13 / 100, bh = H * 6 / 100, y = H - bh - H * 4 / 100, x = W - W * 4 / 100;
    for (int i = 2; i >= 0; i--) { x -= bw; g_btn[i] = R(x, y, x + bw, y + bh); x -= W / 100; int hot = g_nbtn_hot == i;
        pill(dc, g_btn[i], i == 0 ? fill : (hot ? c_mix(fill, txt, 0.2) : c_mix(fill, RGB(0, 0, 0), 0.4)), line, round);
        HGDIOBJ o = SelectObject(dc, g_f2); g_text(dc, labels[i], g_btn[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE, i == 0 ? txt : RGB(235, 235, 235)); SelectObject(dc, o); }
}
static void clock_text(HDC dc, int W, int H, COLORREF c) { struct tm t; clock_tm(&t); wchar_t b[32]; wcsftime(b, 32, L"%I:%M %p", &t); if (b[0] == L'0') memmove(b, b + 1, wcslen(b) * sizeof(wchar_t)); HGDIOBJ o = SelectObject(dc, g_f2); g_text(dc, b, R(W / 2, H / 40, W - W / 30, H / 12), DT_RIGHT | DT_VCENTER | DT_SINGLELINE, c); SelectObject(dc, o); }
static void background(HDC dc, int W, int H, COLORREF top, COLORREF bot, int dim) {
    RECT all = R(0, 0, W, H); g_vgrad(dc, all, top, bot, 4); JNode *g = game_at(g_sel); wchar_t bg[MAX_PATH]; w_from_utf8_buf(gs(g, "bg"), bg, MAX_PATH);
    if (bg[0] && img_fill(dc, bg, W, H, "fill")) g_alpha(dc, all, RGB(0, 0, 0), dim);
}

/* ------------------------------------------------------------------ themes */
static void th_ps5(HDC dc, int W, int H) {
    background(dc, W, H, RGB(10, 22, 58), RGB(0, 70, 150), 120); SetBkMode(dc, TRANSPARENT); clock_text(dc, W, H, RGB(255, 255, 255));
    wtext(dc, g_f2, cfg_str("game_title"), R(W / 14, H / 40, W / 2, H / 12), DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
    int S = H * 17 / 100, gap = S / 4, n = ng(), cy = H * 50 / 100;
    for (int i = 0; i < n; i++) { double d = i - g_pos; int sel = i == g_sel; int s = sel ? S * 13 / 10 : S; int cx = W / 2 + (int)(d * (S + gap)) + (d > 0.4 ? (S * 3 / 10) : (d < -0.4 ? -(S * 3 / 10) : 0)) ; RECT r = R(cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2); g_cell[i] = r;
        if (r.right < 0 || r.left > W) continue; if (sel) { RECT o = r; InflateRect(&o, 4, 4); g_alpha(dc, o, RGB(255, 255, 255), 60); }
        cover(dc, i, r, 0); if (sel) g_frame(dc, r, RGB(255, 255, 255), 3); else g_alpha(dc, r, RGB(0, 0, 0), 70); }
    JNode *g = game_at(g_sel); if (g) { wtext(dc, g_f1, gs(g, "name"), R(W / 14, H * 70 / 100, W * 7 / 10, H * 79 / 100), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, RGB(255, 255, 255));
        wtext(dc, g_f3, gs(g, "desc"), R(W / 14, H * 79 / 100, W * 6 / 10, H * 93 / 100), DT_WORDBREAK | DT_END_ELLIPSIS, RGB(210, 220, 240)); }
    const wchar_t *l[3] = { L"Play", L"Options", L"Exit" }; buttons(dc, W, H, RGB(255, 255, 255), RGB(0, 0, 0), RGB(255, 255, 255), H / 14, l);
}
static void th_steam(HDC dc, int W, int H) {
    background(dc, W, H, RGB(23, 26, 33), RGB(27, 40, 56), 170); SetBkMode(dc, TRANSPARENT); clock_text(dc, W, H, RGB(200, 212, 225));
    int sb = W * 14 / 100; g_fill(dc, R(0, 0, sb, H), RGB(16, 20, 26)); static const wchar_t *items[3] = { L"LIBRARY", L"OPTIONS", L"EXIT" };
    for (int i = 0; i < 3; i++) { HGDIOBJ o = SelectObject(dc, g_f2); g_text(dc, items[i], R(W / 40, H * (20 + i * 6) / 100, sb, H * (26 + i * 6) / 100), DT_VCENTER | DT_SINGLELINE, i == 0 ? RGB(102, 192, 244) : RGB(160, 170, 180)); SelectObject(dc, o); }
    int cwid = W * 11 / 100, chgt = cwid * 3 / 2, gap = W / 90, cols = (W - sb - W / 20) / (cwid + gap); if (cols < 1) cols = 1; int n = ng(), selrow = g_sel / cols, rowh = chgt + gap;
    int roff = selrow * rowh + chgt - H * 68 / 100; if (roff < 0) roff = 0;
    for (int i = 0; i < n; i++) { int col = i % cols, row = i / cols; int x = sb + W / 30 + col * (cwid + gap), y = H * 12 / 100 + row * (chgt + gap) - roff; RECT r = R(x, y, x + cwid, y + chgt); g_cell[i] = r; if (r.bottom < 0 || r.top > H) continue;
        if (i == g_sel) { InflateRect(&r, 6, 9); } cover(dc, i, r, 0); if (i == g_sel) g_frame(dc, r, RGB(102, 192, 244), 3); }
    JNode *g = game_at(g_sel); if (g) { g_alpha(dc, R(sb, H * 80 / 100, W, H), RGB(0, 0, 0), 190); wtext(dc, g_f1, gs(g, "name"), R(sb + W / 30, H * 81 / 100, W * 6 / 10, H * 89 / 100), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, RGB(255, 255, 255));
        wtext(dc, g_f3, gs(g, "desc"), R(sb + W / 30, H * 89 / 100, W * 6 / 10, H * 99 / 100), DT_WORDBREAK | DT_END_ELLIPSIS, RGB(170, 185, 200)); }
    const wchar_t *l[3] = { L"PLAY", L"OPTIONS", L"EXIT" }; buttons(dc, W, H, RGB(90, 168, 50), RGB(255, 255, 255), RGB(90, 168, 50), H / 80, l);
}
static void th_switch(HDC dc, int W, int H) {
    RECT all = R(0, 0, W, H); g_vgrad(dc, all, RGB(240, 240, 240), RGB(225, 225, 228), 4); JNode *gsel = game_at(g_sel); wchar_t bg[MAX_PATH]; w_from_utf8_buf(gs(gsel, "bg"), bg, MAX_PATH); if (bg[0] && img_fill(dc, bg, W, H, "fill")) g_alpha(dc, all, RGB(255, 255, 255), 170);
    SetBkMode(dc, TRANSPARENT); clock_text(dc, W, H, RGB(60, 60, 60)); pill(dc, R(W / 30, H / 40, W / 30 + H / 12, H / 40 + H / 12), RGB(230, 0, 18), RGB(230, 0, 18), H / 12);
    int S = H * 22 / 100, gap = S / 5, n = ng(), cy = H * 42 / 100; double off = g_pos > 2 ? g_pos - 2 : 0;
    for (int i = 0; i < n; i++) { int x = W / 14 + (int)((i - off) * (S + gap)); RECT r = R(x, cy - S / 2, x + S, cy + S / 2); g_cell[i] = r; if (r.right < 0 || r.left > W) continue; cover(dc, i, r, 0);
        if (i == g_sel) { RECT o = r; InflateRect(&o, 6, 6); g_frame(dc, o, RGB(230, 0, 18), 5); wtext(dc, g_f3, gs(game_at(i), "name"), R(r.left - 20, r.top - H / 14, r.right + 20, r.top - 6), DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS, RGB(50, 50, 50)); } }
    if (gsel) wtext(dc, g_f3, gs(gsel, "desc"), R(W / 14, H * 62 / 100, W * 8 / 10, H * 76 / 100), DT_WORDBREAK | DT_END_ELLIPSIS, RGB(70, 70, 70));
    static const int gl[3] = { G_PLAY_, G_GEAR, G_POWER }; int d = H * 8 / 100, x0 = W / 2 - (int)(d * 3.2);
    for (int i = 0; i < 3; i++) { RECT r = R(x0 + i * (int)(d * 1.7), H * 84 / 100, x0 + i * (int)(d * 1.7) + d, H * 84 / 100 + d); g_btn[i] = r; pill(dc, r, g_nbtn_hot == i ? RGB(255, 255, 255) : RGB(250, 250, 250), RGB(170, 170, 170), d); glyph_draw(gl[i], dc, R(r.left + d / 4, r.top + d / 4, r.right - d / 4, r.bottom - d / 4), RGB(70, 70, 70)); }
}
static void th_xbox(HDC dc, int W, int H) {
    background(dc, W, H, RGB(14, 14, 14), RGB(8, 66, 8), 120); SetBkMode(dc, TRANSPARENT); clock_text(dc, W, H, RGB(255, 255, 255)); int n = ng(); JNode *g = game_at(g_sel);
    RECT hero = R(W / 14, H * 18 / 100, W * 52 / 100, H * 66 / 100); if (n) { cover(dc, g_sel, hero, 0); g_frame(dc, hero, RGB(255, 255, 255), 4); g_cell[g_sel] = hero; }
    int s = H * 15 / 100, gap = s / 6, x = W * 56 / 100, y = H * 18 / 100, col = 0;
    for (int i = 0; i < n; i++) { if (i == g_sel) continue; RECT r = R(x + col * (s + gap), y, x + col * (s + gap) + s, y + s); g_cell[i] = r; cover(dc, i, r, 0); g_alpha(dc, r, RGB(0, 0, 0), 60); if (++col >= 3) { col = 0; y += s + gap; } }
    if (g) { wtext(dc, g_f1, gs(g, "name"), R(W / 14, H * 68 / 100, W * 6 / 10, H * 77 / 100), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, RGB(255, 255, 255)); wtext(dc, g_f3, gs(g, "desc"), R(W / 14, H * 77 / 100, W * 6 / 10, H * 93 / 100), DT_WORDBREAK | DT_END_ELLIPSIS, RGB(210, 230, 210)); }
    const wchar_t *l[3] = { L"Play", L"Options", L"Exit" }; buttons(dc, W, H, RGB(16, 124, 16), RGB(255, 255, 255), RGB(120, 200, 120), 6, l);
}
static void th_retro(HDC dc, int W, int H) {
    background(dc, W, H, RGB(12, 12, 30), RGB(40, 20, 60), 150); SetBkMode(dc, TRANSPARENT); clock_text(dc, W, H, RGB(255, 255, 120)); int n = ng(), rh = H * 9 / 100, top = H * 14 / 100 - (g_sel > 6 ? (g_sel - 6) * rh : 0);
    wtext(dc, g_f1, cfg_str("game_title"), R(W / 12, H / 40, W / 2, H / 10), DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 120));
    for (int i = 0; i < n; i++) { RECT r = R(W / 12, top + i * rh, W * 7 / 12, top + (i + 1) * rh - 4); g_cell[i] = r; if (r.bottom < 0 || r.top > H) continue; if (i == g_sel) g_fill(dc, r, RGB(70, 40, 120));
        cover(dc, i, R(r.left + 4, r.top + 4, r.left + rh - 8, r.bottom - 4), 0); wtext(dc, g_f2, gs(game_at(i), "name"), R(r.left + rh, r.top, r.right, r.bottom), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, i == g_sel ? RGB(255, 255, 120) : RGB(220, 220, 240)); }
    JNode *g = game_at(g_sel); if (g) wtext(dc, g_f3, gs(g, "desc"), R(W * 62 / 100, H * 18 / 100, W - W / 12, H * 70 / 100), DT_WORDBREAK | DT_END_ELLIPSIS, RGB(210, 210, 240));
    const wchar_t *l[3] = { L"START", L"OPTIONS", L"EXIT" }; buttons(dc, W, H, RGB(255, 255, 120), RGB(20, 20, 40), RGB(255, 255, 120), 0, l);
}

static void paint(HDC dc, int W, int H) {
    fonts(H); const char *t = cfg_str("game_theme"); for (int i = 0; i < 3; i++) g_btn[i] = R(0, 0, 0, 0); for (int i = 0; i < MAX_G; i++) g_cell[i] = R(0, 0, 0, 0);
    HGDIOBJ of = SelectObject(dc, g_f2);
    if (!strcmp(t, "steam")) th_steam(dc, W, H); else if (!strcmp(t, "switch")) th_switch(dc, W, H); else if (!strcmp(t, "xbox")) th_xbox(dc, W, H); else if (!strcmp(t, "retro")) th_retro(dc, W, H); else th_ps5(dc, W, H);
    if (!ng()) { SetBkMode(dc, TRANSPARENT); wtext(dc, g_f1, "No games yet. Press O (Options) and add games in Settings > Game mode.", R(W / 10, H / 3, W * 9 / 10, H * 2 / 3), DT_CENTER | DT_VCENTER | DT_WORDBREAK, RGB(255, 255, 255)); }
    SelectObject(dc, of);
}

/* ------------------------------------------------------------------ actions */
static void leave(void) { game_close(); }
static void launch(void) {
    JNode *g = game_at(g_sel); if (!g) return; wchar_t p[MAX_PATH]; w_from_utf8_buf(gs(g, "path"), p, MAX_PATH); if (!p[0]) return;
    wchar_t full[MAX_PATH]; find_exe(p, full, MAX_PATH); SHELLEXECUTEINFOW sei; memset(&sei, 0, sizeof sei); sei.cbSize = sizeof sei; sei.fMask = SEE_MASK_NOCLOSEPROCESS; sei.lpFile = full; sei.nShow = SW_SHOWNORMAL;
    wchar_t dir[MAX_PATH]; lstrcpynW(dir, full, MAX_PATH); wchar_t *sl = wcsrchr(dir, L'\\'); if (sl) { *sl = 0; sei.lpDirectory = dir; }
    wchar_t args[300]; w_from_utf8_buf(gs(g, "args"), args, 300); if (args[0]) sei.lpParameters = args;
    if (g_proc) { CloseHandle(g_proc); g_proc = NULL; }
    if (ShellExecuteExW(&sei) && sei.hProcess) g_proc = sei.hProcess; g_open = 0; ShowWindow(g_w, SW_HIDE);
}
static void move(int dx) { int n = ng(); if (!n) return; g_sel += dx; if (g_sel < 0) g_sel = 0; if (g_sel >= n) g_sel = n - 1; InvalidateRect(g_w, NULL, FALSE); }
static void options(void) { leave(); settings_open_page("Game mode"); }
static void act_button(int i) { if (i == 0) launch(); else if (i == 1) options(); else leave(); }
static int cols_steam(int W) { int sb = W * 14 / 100, cwid = W * 11 / 100, gap = W / 90, c = (W - sb - W / 20) / (cwid + gap); return c < 1 ? 1 : c; }
static void key(WPARAM k) {
    RECT rc; GetClientRect(g_w, &rc); int steam = !strcmp(cfg_str("game_theme"), "steam");
    if (k == VK_ESCAPE) leave(); else if (k == VK_RETURN) launch(); else if (k == 'O' || k == 'E') options();
    else if (k == VK_LEFT) move(-1); else if (k == VK_RIGHT) move(1); else if (k == VK_UP) move(steam ? -cols_steam(rc.right) : -1); else if (k == VK_DOWN) move(steam ? cols_steam(rc.right) : 1);
    else if (k == VK_HOME) move(-9999); else if (k == VK_END) move(9999);
}
static void poll_pad(void) {
    if (!g_xtried) { g_xtried = 1; static const wchar_t *dlls[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" }; for (int i = 0; i < 3 && !g_xget; i++) { HMODULE m = LoadLibraryW(dlls[i]); if (m) g_xget = (XGet_t)GetProcAddress(m, "XInputGetState"); } }
    if (!g_xget) return; XState st; memset(&st, 0, sizeof st); if (g_xget(0, &st) != 0) return; WORD b = st.pad.wButtons; if (st.pad.lx < -16000) b |= 4; if (st.pad.lx > 16000) b |= 8; if (st.pad.ly > 16000) b |= 1; if (st.pad.ly < -16000) b |= 2;
    WORD edge = (WORD)(b & ~g_xprev); DWORD now = GetTickCount(); if (b == g_xprev && now - g_xlast > 220 && (b & 15)) { edge = (WORD)(b & 15); g_xlast = now; } if (edge != 0 && b != g_xprev) g_xlast = now; g_xprev = b;
    if (edge & 4) key(VK_LEFT); if (edge & 8) key(VK_RIGHT); if (edge & 1) key(VK_UP); if (edge & 2) key(VK_DOWN); if (edge & 0x1000) key(VK_RETURN); if (edge & 0x2000) key(VK_ESCAPE); if (edge & 0x8000) key('O');
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: { PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc); HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        paint(mem, rc.right, rc.bottom); BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER: { poll_pad(); if (g_proc && WaitForSingleObject(g_proc, 0) == WAIT_OBJECT_0) { CloseHandle(g_proc); g_proc = NULL; if (cfg_bool("game_return")) game_open(); }
        if (g_open) { double d = g_sel - g_pos; if (d > 0.01 || d < -0.01) { g_pos += d * 0.22; InvalidateRect(h, NULL, FALSE); } else if (g_pos != g_sel) { g_pos = g_sel; InvalidateRect(h, NULL, FALSE); }
            if (GetForegroundWindow() != h) SetForegroundWindow(h); } return 0; }
    case WM_KEYDOWN: key(w); return 0;
    case WM_MOUSEMOVE: { int x = (short)LOWORD(l), y = (short)HIWORD(l), hb = -1; for (int i = 0; i < 3; i++) if (in_rect(&g_btn[i], x, y)) hb = i; if (hb != g_nbtn_hot) { g_nbtn_hot = hb; InvalidateRect(h, NULL, FALSE); } return 0; }
    case WM_MOUSEWHEEL: move(-(short)HIWORD(w) / WHEEL_DELTA); return 0;
    case WM_LBUTTONUP: { int x = (short)LOWORD(l), y = (short)HIWORD(l); for (int i = 0; i < 3; i++) if (in_rect(&g_btn[i], x, y)) { act_button(i); return 0; }
        for (int i = ng() - 1; i >= 0; i--) if (in_rect(&g_cell[i], x, y)) { if (i == g_sel) launch(); else { g_sel = i; InvalidateRect(h, NULL, FALSE); } return 0; } return 0; }
    case WM_RBUTTONUP: leave(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void game_open(void) {
    if (!g_w) { WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(100)); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
        int sw, sh; screen_size(&sw, &sh); g_w = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLASS_NAME, L"WinShell Game Mode", WS_POPUP, 0, 0, sw, sh, NULL, NULL, wc.hInstance, NULL); if (!g_w) return; SetTimer(g_w, 1, 16, NULL); }
    int sw, sh; screen_size(&sw, &sh); if (g_sel >= ng()) g_sel = 0; g_pos = g_sel; g_open = 1; start_close(); preview_close();
    SetWindowPos(g_w, HWND_TOPMOST, 0, 0, sw, sh, 0); ShowWindow(g_w, SW_SHOW); SetForegroundWindow(g_w); SetFocus(g_w); InvalidateRect(g_w, NULL, FALSE);
}
void game_close(void) { g_open = 0; if (g_w) ShowWindow(g_w, SW_HIDE); }
void game_apply(void) {
    if (g_w) InvalidateRect(g_w, NULL, FALSE);
    if (cfg_bool("game_autostart") && !g_open && !g_w) game_open();
}
void game_destroy(void) { if (g_proc) { CloseHandle(g_proc); g_proc = NULL; } if (g_w) { DestroyWindow(g_w); g_w = NULL; } if (g_f1) DeleteObject(g_f1); if (g_f2) DeleteObject(g_f2); if (g_f3) DeleteObject(g_f3); g_f1 = g_f2 = g_f3 = NULL; }
