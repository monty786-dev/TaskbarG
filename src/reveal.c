/* Reveal bar: a thin strip on the taskbar's screen edge. Hover or click it to bring an auto-hidden
 * taskbar back. Works for horizontal and vertical (left / right) taskbars. Opacity can go to 0. */
#include "app.h"
#include <string.h>

#define CLASS_NAME L"WinShellReveal"
static HWND g_w; static int g_visible;

static RECT strip_rect(void) {
    int sw, sh; screen_size(&sw, &sh); int t = px(cfg_int("strip_height")), pct = cfg_int("strip_width"); if (pct < 5) pct = 5; if (pct > 100) pct = 100;
    const char *pos = cfg_str("position"), *al = cfg_str("strip_pos"); RECT r;
    if (bar_vertical()) { int len = sh * pct / 100, y = !strcmp(al, "start") ? 0 : (!strcmp(al, "end") ? sh - len : (sh - len) / 2);
        if (!strcmp(pos, "left")) r = (RECT){ 0, y, t, y + len }; else r = (RECT){ sw - t, y, sw, y + len }; }
    else { int len = sw * pct / 100, x = !strcmp(al, "start") ? 0 : (!strcmp(al, "end") ? sw - len : (sw - len) / 2);
        if (!strcmp(pos, "top")) r = (RECT){ x, 0, x + len, t }; else r = (RECT){ x, sh - t, x + len, sh }; }
    return r;
}
static COLORREF strip_color(void) { return cfg_str("strip_color")[0] ? c_hex(cfg_str("strip_color"), app_accent()) : app_accent(); }

static LRESULT CALLBACK reveal_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc); g_fill(dc, rc, strip_color()); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_MOUSEMOVE: case WM_LBUTTONDOWN: bar_reveal(cfg_int("strip_seconds")); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void reveal_apply(void) {
    if (!g_w) return;
    RECT r = strip_rect(); SetWindowPos(g_w, HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE);
    int a = 255 * cfg_int("strip_opacity") / 100; if (a < 1) a = 1;           /* 0 % = invisible, but still reacts to the mouse */
    apply_alpha(g_w, a); InvalidateRect(g_w, NULL, FALSE);
}
void reveal_show(int on) {
    if (on && !cfg_bool("strip_enabled")) on = 0;
    if (on == g_visible) return;
    g_visible = on;
    if (on) {
        if (!g_w) {
            WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = reveal_proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_HAND); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
            g_w = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, CLASS_NAME, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
            if (!g_w) { g_visible = 0; return; }
        }
        reveal_apply(); ShowWindow(g_w, SW_SHOWNA);
    } else if (g_w) ShowWindow(g_w, SW_HIDE);
}
void reveal_destroy(void) { if (g_w) { DestroyWindow(g_w); g_w = NULL; } g_visible = 0; }
