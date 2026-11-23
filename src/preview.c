/* "All running windows" flyout: live thumbnails of every window with
 * go-to (click), close, pin (always on top) and "Close all windows". */
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLASS_NAME L"WinShellPreview"
#define MAX_CARDS 40
typedef struct { HWND h; HBITMAP bmp; int has; wchar_t title[96]; RECT card, closeR, pinR; } Card;
static Card g_c[MAX_CARDS]; static int g_n, g_scroll, g_hover = -1, g_part, g_content;
static HWND g_wnd; static int g_visible; static RECT g_hdr_all;

static int cw(void) { int v = cfg_int("preview_w"); return px(v < 120 ? 120 : (v > 480 ? 480 : v)); }
static int ch(void) { return cw() * 62 / 100; }
static int TH(void) { return px(30); }
static RECT RC(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }

static void capture(Card *c) {
    if (!IsWindow(c->h) || IsIconic(c->h)) return;
    RECT r; GetWindowRect(c->h, &r); int w = r.right - r.left, h = r.bottom - r.top; if (w < 8 || h < 8 || w > 5000 || h > 5000) return;
    HDC dc = GetDC(NULL), mem = CreateCompatibleDC(dc); HBITMAP full = CreateCompatibleBitmap(dc, w, h); HGDIOBJ old = SelectObject(mem, full);
    BOOL ok = PrintWindow(c->h, mem, 0) || PrintWindow(c->h, mem, 1);
    if (ok) {
        int tw = cw(), th = ch(); double k = (double)tw / w < (double)th / h ? (double)tw / w : (double)th / h; int dw = (int)(w * k), dh = (int)(h * k);
        HDC t = CreateCompatibleDC(dc); HBITMAP tb = CreateCompatibleBitmap(dc, tw, th); HGDIOBJ ot = SelectObject(t, tb);
        Theme thm = app_theme(1); g_fill(t, RC(0, 0, tw, th), c_mix(thm.bg, RGB(0, 0, 0), 0.25));
        SetStretchBltMode(t, HALFTONE); StretchBlt(t, (tw - dw) / 2, (th - dh) / 2, dw, dh, mem, 0, 0, w, h, SRCCOPY);
        SelectObject(t, ot); DeleteDC(t); if (c->bmp) DeleteObject(c->bmp); c->bmp = tb; c->has = 1;
    }
    SelectObject(mem, old); DeleteObject(full); DeleteDC(mem); ReleaseDC(NULL, dc);
}

static void layout(int *W, int *H) {
    int sw, sh; screen_size(&sw, &sh); int gp = px(12), hdr = px(46), cols = g_n < 1 ? 1 : g_n; int maxc = (sw * 9 / 10 - gp) / (cw() + gp); if (maxc < 1) maxc = 1; if (maxc > 5) maxc = 5; if (cols > maxc) cols = maxc;
    int cardh = ch() + TH(), rows = (g_n + cols - 1) / (cols ? cols : 1); *W = cols * (cw() + gp) + gp; if (*W < px(300)) *W = px(300);
    g_content = rows * (cardh + gp) + gp; int maxh = sh * 7 / 10; *H = hdr + (g_content < maxh ? g_content : maxh);
    for (int i = 0; i < g_n; i++) {
        int col = i % cols, row = i / cols; int x = gp + col * (cw() + gp), y = hdr + gp + row * (cardh + gp) - g_scroll;
        Card *c = &g_c[i]; c->card = RC(x, y, x + cw(), y + cardh);
        c->closeR = RC(c->card.right - px(30), c->card.top, c->card.right, c->card.top + TH()); c->pinR = RC(c->closeR.left - px(30), c->card.top, c->closeR.left, c->card.top + TH());
    }
    g_hdr_all = RC(*W - px(190), px(8), *W - px(10), hdr - px(8));
}

static void rebuild(int recapture) {
    HWND list[MAX_CARDS]; int n = bar_task_windows(list, MAX_CARDS); static Card old[MAX_CARDS]; memcpy(old, g_c, sizeof g_c); int oldn = g_n; g_n = 0;
    for (int i = 0; i < n; i++) {
        Card *c = &g_c[g_n]; memset(c, 0, sizeof *c); c->h = list[i];
        for (int k = 0; k < oldn; k++) if (old[k].h == list[i]) { c->bmp = old[k].bmp; c->has = old[k].has; old[k].bmp = NULL; }
        GetWindowTextW(c->h, c->title, 96); if (recapture || !c->has) capture(c); g_n++;
    }
    for (int k = 0; k < oldn; k++) if (old[k].bmp) DeleteObject(old[k].bmp);
}

static void draw(HDC dc, int W, int H) {
    Theme th = app_theme(1); g_fill(dc, RC(0, 0, W, H), th.bg); SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font);
    wchar_t t[64]; _snwprintf(t, 64, L"%d open window%ls", g_n, g_n == 1 ? L"" : L"s"); HGDIOBJ o = SelectObject(dc, g_font_big); g_text(dc, t, RC(px(14), 0, W / 2, px(46)), DT_VCENTER | DT_SINGLELINE, th.text); SelectObject(dc, o);
    int hot = g_hover == -2; g_fill(dc, g_hdr_all, hot ? c_mix(th.bg, RGB(232, 17, 35), 0.8) : th.input); g_frame(dc, g_hdr_all, hot ? RGB(232, 17, 35) : th.active, 1);
    g_text(dc, L"Close all windows", g_hdr_all, DT_CENTER | DT_VCENTER | DT_SINGLELINE, hot ? RGB(255, 255, 255) : th.text);
    HRGN rgn = CreateRectRgn(0, px(46), W, H); SelectClipRgn(dc, rgn);
    for (int i = 0; i < g_n; i++) {
        Card *c = &g_c[i]; if (c->card.bottom < px(46) || c->card.top > H) continue; int sel = g_hover == i;
        g_fill(dc, c->card, c_mix(th.bg, th.text, sel ? 0.14 : 0.07)); if (sel) g_frame(dc, c->card, th.accent, 2);
        HICON ic = icon_for_window(c->h, L""); int isz = px(16); DrawIconEx(dc, c->card.left + px(8), c->card.top + (TH() - isz) / 2, ic, isz, isz, 0, NULL, DI_NORMAL);
        g_text(dc, c->title, RC(c->card.left + px(30), c->card.top, c->pinR.left - px(2), c->card.top + TH()), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th.text);
        RECT tr = RC(c->card.left, c->card.top + TH(), c->card.right, c->card.bottom);
        if (c->has && c->bmp) { HDC m = CreateCompatibleDC(dc); HGDIOBJ ob = SelectObject(m, c->bmp); BitBlt(dc, tr.left, tr.top, cw(), ch(), m, 0, 0, SRCCOPY); SelectObject(m, ob); DeleteDC(m); }
        else { g_fill(dc, tr, c_mix(th.bg, RGB(0, 0, 0), 0.25)); int s = px(48); DrawIconEx(dc, (tr.left + tr.right - s) / 2, (tr.top + tr.bottom - s) / 2, ic, s, s, 0, NULL, DI_NORMAL); }
        if (IsIconic(c->h)) g_text(dc, L"minimized", tr, DT_CENTER | DT_BOTTOM | DT_SINGLELINE, th.dim);
        if (sel) { if (g_part == 2) g_fill(dc, c->closeR, RGB(232, 17, 35)); if (g_part == 1) g_alpha(dc, c->pinR, th.text, 40);
            glyph_draw(G_CLOSE, dc, RC(c->closeR.left + px(8), c->closeR.top + px(8), c->closeR.right - px(8), c->closeR.bottom - px(8)), g_part == 2 ? RGB(255, 255, 255) : th.text);
            int top = (GetWindowLongPtrW(c->h, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0; glyph_draw(G_PIN, dc, RC(c->pinR.left + px(7), c->pinR.top + px(7), c->pinR.right - px(7), c->pinR.bottom - px(7)), top ? th.accent : th.text); }
    }
    SelectClipRgn(dc, NULL); DeleteObject(rgn); g_frame(dc, RC(0, 0, W, H), th.active, 1); SelectObject(dc, of);
}

static void hit(int x, int y, int *idx, int *part) {
    *idx = -1; *part = 0; if (in_rect(&g_hdr_all, x, y)) { *idx = -2; return; }
    for (int i = 0; i < g_n; i++) if (in_rect(&g_c[i].card, x, y)) { *idx = i; *part = in_rect(&g_c[i].closeR, x, y) ? 2 : (in_rect(&g_c[i].pinR, x, y) ? 1 : 0); return; }
}

int preview_visible(void) { return g_visible; }
HWND preview_hwnd(void) { return g_wnd; }
void preview_close(void) { if (!g_visible) return; g_visible = 0; KillTimer(g_wnd, 1); ShowWindow(g_wnd, SW_HIDE); }

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int hd; LRESULT mr;
    switch (m) {
    case WM_PAINT: { PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc); int W, H; layout(&W, &H);
        HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp); draw(mem, rc.right, rc.bottom);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_MEASUREITEM: case WM_DRAWITEM: mr = app_menu_msg(m, w, l, &hd); if (hd) return mr; break;
    case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE && g_visible && g_menu_depth == 0) preview_close(); return 0;
    case WM_KEYDOWN: if (w == VK_ESCAPE) preview_close(); return 0;
    case WM_TIMER: { int keep = g_scroll; rebuild(1); int W, H; layout(&W, &H); g_scroll = keep; if (!g_n) { preview_close(); return 0; } InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_MOUSEMOVE: { int i, p; hit((short)LOWORD(l), (short)HIWORD(l), &i, &p); if (i != g_hover || p != g_part) { g_hover = i; g_part = p; InvalidateRect(h, NULL, FALSE); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); return 0; }
    case WM_MOUSELEAVE: g_hover = -1; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_MOUSEWHEEL: { int W, H, d = -(short)HIWORD(w) / WHEEL_DELTA * px(60); layout(&W, &H); int mx = g_content - (H - px(46)); if (mx < 0) mx = 0; g_scroll += d; if (g_scroll > mx) g_scroll = mx; if (g_scroll < 0) g_scroll = 0; InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_LBUTTONUP: { int i, p; int W, H; layout(&W, &H); hit((short)LOWORD(l), (short)HIWORD(l), &i, &p);
        if (i == -2) { if (g_n && app_confirm(h, L"Close all windows", L"Close every open window?")) { for (int k = 0; k < g_n; k++) anim_window(g_c[k].h, 1); preview_close(); } }
        else if (i >= 0) { HWND t = g_c[i].h;
            if (p == 2) { anim_window(t, 1); SetTimer(h, 1, 350, NULL); }
            else if (p == 1) { SetWindowPos(t, (GetWindowLongPtrW(t, GWL_EXSTYLE) & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); InvalidateRect(h, NULL, FALSE); }
            else { preview_close(); if (IsIconic(t)) ShowWindow(t, SW_RESTORE); SetForegroundWindow(t); } }
        return 0; }
    case WM_RBUTTONUP: { int i, p; hit((short)LOWORD(l), (short)HIWORD(l), &i, &p); if (i >= 0) { HWND t = g_c[i].h; const wchar_t *lab[] = { L"Go to window", L"Minimize", L"Close window", L"End task (force quit)" }; int id[] = { 1, 2, 3, 4 }, fl[] = { 0, 0, 0, 0 };
            POINT pt; GetCursorPos(&pt); int cmd = app_popup(h, lab, id, fl, 4, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y);
            if (cmd == 1) { preview_close(); if (IsIconic(t)) ShowWindow(t, SW_RESTORE); SetForegroundWindow(t); } else if (cmd == 2) ShowWindow(t, SW_MINIMIZE); else if (cmd == 3) { anim_window(t, 1); SetTimer(h, 1, 350, NULL); }
            else if (cmd == 4) { DWORD pid = 0; wchar_t prm[64]; GetWindowThreadProcessId(t, &pid); _snwprintf(prm, 64, L"/F /PID %lu", (unsigned long)pid); app_run(L"taskkill.exe", prm, SW_HIDE); SetTimer(h, 1, 500, NULL); } } return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}

void preview_toggle(void) {
    if (g_visible) { preview_close(); return; }
    if (!g_wnd) { WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
        g_wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLASS_NAME, L"WinShell Windows", WS_POPUP, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL); if (!g_wnd) return; }
    g_scroll = 0; g_hover = -1; rebuild(1); if (!g_n) return;
    int W, H, sw, sh; layout(&W, &H); screen_size(&sw, &sh); RECT b = bar_rect(); POINT pt; GetCursorPos(&pt); int x, y; const char *pos = cfg_str("position");
    if (bar_vertical()) { x = !strcmp(pos, "left") ? b.right + px(8) : b.left - W - px(8); y = pt.y - H / 2; } else { x = pt.x - W / 2; y = !strcmp(pos, "top") ? b.bottom + px(8) : b.top - H - px(8); }
    if (x + W > sw - 4) x = sw - W - 4; if (x < 4) x = 4; if (y + H > sh - 4) y = sh - H - 4; if (y < 4) y = 4;
    apply_glass(g_wnd, cfg_bool("glass"), cfg_int("glass_opacity")); SetWindowPos(g_wnd, HWND_TOPMOST, x, y, W, H, 0); g_visible = 1; ShowWindow(g_wnd, SW_SHOW); SetForegroundWindow(g_wnd);
    SetTimer(g_wnd, 1, 1500, NULL); InvalidateRect(g_wnd, NULL, FALSE);
}
void preview_destroy(void) { if (g_wnd) { DestroyWindow(g_wnd); g_wnd = NULL; } }
