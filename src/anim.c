/* Slide / fade animations for the taskbar, the Start panel and other programs' windows. */
#include "app.h"
#include <string.h>

#define MAX_ANIM 24
typedef struct {
    HWND h; RECT from, to, restore; DWORD t0; int ms, a_from, a_to, end, on; DWORD late;
} Anim;
static Anim g_a[MAX_ANIM];
static HWND g_timer_wnd;
static int g_running;

static LRESULT CALLBACK anim_proc(HWND h, UINT m, WPARAM w, LPARAM l);

static void ensure(void) {
    if (g_timer_wnd) return;
    WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = anim_proc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"WinShellAnim";
    RegisterClassW(&wc);
    g_timer_wnd = CreateWindowExW(0, L"WinShellAnim", L"", WS_POPUP, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL);
}

int anim_busy(HWND h) { for (int i = 0; i < MAX_ANIM; i++) if (g_a[i].on && g_a[i].h == h) return 1; return 0; }

static void put(Anim *a, RECT r, int alpha) {
    SetWindowPos(a->h, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    if (alpha >= 0) SetLayeredWindowAttributes(a->h, 0, (BYTE)alpha, LWA_ALPHA);
}

static void finish(Anim *a) {
    a->on = 0;
    if (!IsWindow(a->h)) return;
    if (a->end == ANIM_END_HIDE) { ShowWindow(a->h, SW_HIDE); put(a, a->restore, a->a_to < 0 ? -1 : a->a_to); }
    else if (a->end == ANIM_END_CLOSE) { PostMessageW(a->h, WM_CLOSE, 0, 0); a->on = 1; a->late = GetTickCount() + 600; if (!a->late) a->late = 1; }   /* snap back if the program refuses to close */
    else put(a, a->to, a->a_to);
}

static void tick(void) {
    int any = 0; DWORD now = GetTickCount();
    for (int i = 0; i < MAX_ANIM; i++) {
        Anim *a = &g_a[i]; if (!a->on) continue;
        if (!IsWindow(a->h)) { a->on = 0; continue; }
        if (a->late) { if ((int)(now - a->late) >= 0) { a->late = 0; a->on = 0; put(a, a->restore, -1); } else any = 1; continue; }
        double t = a->ms > 0 ? (double)(now - a->t0) / a->ms : 1; if (t > 1) t = 1;
        double e = 1 - (1 - t) * (1 - t) * (1 - t);
        if (t >= 1) { finish(a); if (a->on) any = 1; continue; }
        RECT r; r.left = a->from.left + (int)((a->to.left - a->from.left) * e); r.top = a->from.top + (int)((a->to.top - a->from.top) * e);
        r.right = a->from.right + (int)((a->to.right - a->from.right) * e); r.bottom = a->from.bottom + (int)((a->to.bottom - a->from.bottom) * e);
        int al = a->a_from < 0 || a->a_to < 0 ? -1 : a->a_from + (int)((a->a_to - a->a_from) * e);
        put(a, r, al); any = 1;
    }
    if (!any && g_running) { KillTimer(g_timer_wnd, 1); g_running = 0; }
}

static LRESULT CALLBACK anim_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TIMER) { tick(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

void anim_run(HWND h, RECT from, RECT to, int ms, int a_from, int a_to, int end, RECT restore) {
    ensure();
    Anim *slot = NULL;
    for (int i = 0; i < MAX_ANIM; i++) if (g_a[i].on && g_a[i].h == h) slot = &g_a[i];
    for (int i = 0; i < MAX_ANIM && !slot; i++) if (!g_a[i].on) slot = &g_a[i];
    if (!slot) return;
    slot->h = h; slot->from = from; slot->to = to; slot->restore = restore; slot->t0 = GetTickCount();
    slot->late = 0; slot->ms = ms < 20 ? 20 : ms; slot->a_from = a_from; slot->a_to = a_to; slot->end = end; slot->on = 1;
    put(slot, from, a_from);
    if (!g_running) { SetTimer(g_timer_wnd, 1, 15, NULL); g_running = 1; }
}

void anim_cancel(HWND h) { for (int i = 0; i < MAX_ANIM; i++) if (g_a[i].on && g_a[i].h == h) g_a[i].on = 0; }

/* A rectangle pushed off to one side. dir: left|right|up|down (the side it slides in from). */
RECT anim_offset(RECT r, const char *dir, int dist) {
    int dx = 0, dy = 0;
    if (!strcmp(dir, "left")) dx = -dist; else if (!strcmp(dir, "right")) dx = dist;
    else if (!strcmp(dir, "up")) dy = -dist; else if (!strcmp(dir, "down")) dy = dist;
    OffsetRect(&r, dx, dy); return r;
}

/* Direction a bar at screen edge `pos` slides in from. */
const char *anim_edge_dir(const char *pos) {
    if (!strcmp(pos, "top")) return "up"; if (!strcmp(pos, "left")) return "left"; if (!strcmp(pos, "right")) return "right"; return "down";
}

void anim_window(HWND h, int closing) {
    const char *k_on = closing ? "anim_app_close_on" : "anim_app_on";
    if (!cfg_bool("animations") || !cfg_bool(k_on)) { if (closing) PostMessageW(h, WM_CLOSE, 0, 0); return; }
    RECT r; if (!GetWindowRect(h, &r) || IsIconic(h) || IsZoomed(h)) { if (closing) PostMessageW(h, WM_CLOSE, 0, 0); return; }
    const char *dir = cfg_str(closing ? "anim_app_close_dir" : "anim_app_dir"); int ms = cfg_int(closing ? "anim_app_close_ms" : "anim_app_ms");
    int sw, sh; screen_size(&sw, &sh);
    int dist = (!strcmp(dir, "left") || !strcmp(dir, "right")) ? (sw < 480 ? sw : 480) : (sh < 360 ? sh : 360);
    dist = dist * 2 / 3;
    RECT off = anim_offset(r, dir, dist);
    if (closing) anim_run(h, r, off, ms, -1, -1, ANIM_END_CLOSE, r);
    else anim_run(h, off, r, ms, -1, -1, ANIM_END_NONE, r);
}
