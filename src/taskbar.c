#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLASS_NAME L"WinShellBar"
#define MAX_WINS 256

HWND g_bar_hwnd;
int  g_menu_depth;

static Entry g_entries[MAX_ENTRIES];
static int   g_nent;

typedef struct { HWND h; wchar_t exe[MAX_PATH]; wchar_t key[MAX_PATH]; } WinInfo;
static WinInfo g_wins[MAX_WINS]; static int g_nwins;
static struct { HWND h; wchar_t exe[MAX_PATH]; } g_wexe[MAX_WINS]; static int g_nwexe;

enum { H_NONE, H_START, H_SEARCH, H_DESK, H_CLOSEALL, H_TRAY, H_CLOCK, H_BAR, H_ENTRY };

static int  g_hover = H_NONE; static wchar_t g_hover_key[MAX_PATH];
static int  g_tracking;
static HWND g_flash[32]; static int g_nflash;
static HWND g_hidden[MAX_WINS]; static int g_nhidden;
static RECT g_orig_work; static int g_have_orig;
static int  g_press; static wchar_t g_press_key[MAX_PATH]; static int g_press_x;
static int  g_drag; static wchar_t g_drag_key[MAX_PATH]; static int g_drag_dx, g_drag_x;
static wchar_t g_clock_text[64];
static HWND g_last_fg;
static int  g_tick, g_fast;
static int  g_bar_hidden, g_need_hard; static DWORD g_reveal_until; static int g_reveal_len = 5;
static RECT g_final;                       /* where the bar sits when shown */
static int  g_ed_el = -1, g_ed_sx, g_ed_sy, g_ed_dx, g_ed_dy;   /* edit-layout drag */
static HWND g_pending[16]; static DWORD g_pending_t[16]; static int g_npending;
int g_edit_layout;
static UINT g_shellhook_msg;

typedef struct { RECT r[E_COUNT]; RECT ent[MAX_ENTRIES]; } Layout;

/* ------------------------------------------------------------- helpers */
static int is_ignored_class(const wchar_t *c) {
    static const wchar_t *list[] = { L"Progman", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"WorkerW",
        L"WinShellBar", L"WinShellStart", L"WinShellDesktop", L"#32769", L"Program Manager" };
    for (size_t i = 0; i < sizeof list / sizeof list[0]; i++) if (!wcscmp(c, list[i])) return 1;
    return 0;
}
static int abs_path(const wchar_t *p) { return (p[0] && p[1] == L':') || (p[0] == L'\\' && p[1] == L'\\'); }
static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int rw(const RECT *r) { return r->right - r->left; }

static void client(int *w, int *h) { RECT rc = { 0, 0, 0, 0 }; GetClientRect(g_bar_hwnd, &rc); *w = rc.right; *h = rc.bottom; }
HWND bar_hwnd(void) { return g_bar_hwnd; }
void bar_invalidate(void) { if (g_bar_hwnd) InvalidateRect(g_bar_hwnd, NULL, FALSE); }

static Entry *find_entry(const wchar_t *key) {
    for (int i = 0; i < g_nent; i++) if (!wcscmp(g_entries[i].key, key)) return &g_entries[i];
    return NULL;
}
static int entry_index(const wchar_t *key) {
    for (int i = 0; i < g_nent; i++) if (!wcscmp(g_entries[i].key, key)) return i;
    return -1;
}

static int is_flashing(HWND h) { for (int i = 0; i < g_nflash; i++) if (g_flash[i] == h) return 1; return 0; }
static void flash_add(HWND h) { if (!is_flashing(h) && g_nflash < 32) g_flash[g_nflash++] = h; }
static void flash_del(HWND h) { for (int i = 0; i < g_nflash; i++) if (g_flash[i] == h) { g_flash[i] = g_flash[--g_nflash]; return; } }

static int is_active_hwnd(HWND h) {
    HWND fg = GetForegroundWindow();
    return fg && (fg == h || GetAncestor(fg, GA_ROOTOWNER) == h);
}
static int entry_active(const Entry *e) { for (int i = 0; i < e->ntasks; i++) if (is_active_hwnd(e->tasks[i])) return 1; return 0; }
static HWND primary(const Entry *e) {
    for (int i = 0; i < e->ntasks; i++) if (is_active_hwnd(e->tasks[i])) return e->tasks[i];
    return e->ntasks ? e->tasks[0] : NULL;
}

/* ------------------------------------------------------ work area / pins */
static void apply_work_area(void) {
    if (!g_have_orig) { SystemParametersInfoW(SPI_GETWORKAREA, 0, &g_orig_work, 0); g_have_orig = 1; }
    if (!cfg_bool("reserve_work_area")) { bar_restore_work_area(); return; }
    RECT rc = work_rect(); SystemParametersInfoW(SPI_SETWORKAREA, 0, &rc, SPIF_SENDCHANGE);
}
void bar_restore_work_area(void) {
    if (g_have_orig) SystemParametersInfoW(SPI_SETWORKAREA, 0, &g_orig_work, SPIF_SENDCHANGE);
}

void bar_rebuild_pins(void) {
    static Entry old[MAX_ENTRIES], nw[MAX_ENTRIES];
    char used[MAX_ENTRIES]; int nold = g_nent, nn = 0;
    memcpy(old, g_entries, sizeof(Entry) * (size_t)nold); memset(used, 0, sizeof used);
    JNode *pins = cfg_node("pinned");
    if (pins && pins->t == J_ARR) {
        for (JNode *c = pins->child; c && nn < MAX_ENTRIES; c = c->next) {
            if (!c->s) continue;
            wchar_t p[MAX_PATH], path[MAX_PATH], ex[MAX_PATH], key[MAX_PATH];
            w_from_utf8_buf(c->s, p, MAX_PATH);
            if (abs_path(p)) lstrcpyW(path, p); else find_exe(p, path, MAX_PATH);
            exe_for(path, ex, MAX_PATH); app_key(ex, key, MAX_PATH);
            int found = -1;
            for (int i = 0; i < nold; i++) if (!used[i] && !wcscmp(old[i].key, key)) { found = i; break; }
            if (found >= 0) { used[found] = 1; nw[nn] = old[found]; }
            else { memset(&nw[nn], 0, sizeof(Entry)); lstrcpyW(nw[nn].key, key); }
            nw[nn].pinned = 1; lstrcpyW(nw[nn].path, path); nn++;
        }
    }
    for (int i = 0; i < nold && nn < MAX_ENTRIES; i++)
        if (!used[i] && old[i].ntasks > 0) { nw[nn] = old[i]; nw[nn].pinned = 0; nn++; }
    memcpy(g_entries, nw, sizeof(Entry) * (size_t)nn); g_nent = nn;
}

void bar_pin(const wchar_t *path) {
    wchar_t ex[MAX_PATH], key[MAX_PATH]; exe_for(path, ex, MAX_PATH); app_key(ex, key, MAX_PATH);
    JNode *pins = cfg_node("pinned");
    if (!pins || pins->t != J_ARR) return;
    for (JNode *c = pins->child; c; c = c->next) {
        wchar_t p[MAX_PATH], pe[MAX_PATH], pk[MAX_PATH];
        w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe, MAX_PATH); app_key(pe, pk, MAX_PATH);
        if (!wcscmp(pk, key)) return;
    }
    char *u = utf8_from_w(path); json_push(pins, json_str(u)); free(u);
    app_save(); bar_rebuild_pins(); bar_refresh(1); bar_invalidate();
}

void bar_unpin(const wchar_t *key) {
    JNode *pins = cfg_node("pinned"); if (!pins || pins->t != J_ARR) return;
    JNode *c = pins->child, *prev = NULL;
    while (c) {
        wchar_t p[MAX_PATH], pe[MAX_PATH], pk[MAX_PATH];
        w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe, MAX_PATH); app_key(pe, pk, MAX_PATH);
        JNode *next = c->next;
        if (!wcscmp(pk, key)) { if (prev) prev->next = next; else pins->child = next; c->next = NULL; json_free(c); }
        else prev = c;
        c = next;
    }
    app_save(); bar_rebuild_pins(); bar_refresh(1); bar_invalidate();
}

/* ------------------------------------------------------- window tracking */
static int is_task_window(HWND h) {
    if (h == g_bar_hwnd || !IsWindowVisible(h)) return 0;
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) && !(ex & WS_EX_APPWINDOW)) return 0;
    if (GetWindow(h, GW_OWNER) && !(ex & WS_EX_APPWINDOW)) return 0;
    wchar_t cls[128]; GetClassNameW(h, cls, 128);
    if (is_ignored_class(cls)) return 0;
    return GetWindowTextLengthW(h) > 0;
}

static BOOL CALLBACK enum_windows(HWND h, LPARAM lp) {
    (void)lp;
    if (h && g_nwins < MAX_WINS && is_task_window(h)) g_wins[g_nwins++].h = h;
    return TRUE;
}

static const wchar_t *cached_exe(HWND h) {
    for (int i = 0; i < g_nwexe; i++) if (g_wexe[i].h == h) return g_wexe[i].exe;
    if (g_nwexe >= MAX_WINS) g_nwexe = 0;
    g_wexe[g_nwexe].h = h; window_process_path(h, g_wexe[g_nwexe].exe, MAX_PATH);
    return g_wexe[g_nwexe++].exe;
}

void bar_refresh(int force) {
    g_nwins = 0; EnumWindows(enum_windows, 0);
    /* prune exe cache */
    for (int i = 0; i < g_nwexe; ) {
        int alive = 0; for (int j = 0; j < g_nwins; j++) if (g_wins[j].h == g_wexe[i].h) { alive = 1; break; }
        if (!alive) g_wexe[i] = g_wexe[--g_nwexe]; else i++;
    }
    for (int i = 0; i < g_nwins; i++) {
        const wchar_t *exe = cached_exe(g_wins[i].h);
        lstrcpyW(g_wins[i].exe, exe);
        if (exe[0]) app_key(exe, g_wins[i].key, MAX_PATH);
        else _snwprintf(g_wins[i].key, MAX_PATH, L"hwnd:%ld", (long)(intptr_t)g_wins[i].h);
    }
    int changed = 0;
    for (int i = 0; i < g_nent; i++) {
        Entry *e = &g_entries[i]; HWND tasks[MAX_TASKS]; int nt = 0; const wchar_t *first_exe = NULL;
        for (int j = 0; j < g_nwins && nt < MAX_TASKS; j++)
            if (!wcscmp(g_wins[j].key, e->key)) { if (!nt) first_exe = g_wins[j].exe; tasks[nt++] = g_wins[j].h; }
        if (nt != e->ntasks || memcmp(tasks, e->tasks, sizeof(HWND) * (size_t)nt) != 0) {
            memcpy(e->tasks, tasks, sizeof(HWND) * (size_t)nt); e->ntasks = nt; changed = 1;
        }
        if (nt && first_exe && first_exe[0] && !e->pinned) lstrcpyW(e->path, first_exe);
    }
    for (int j = 0; j < g_nwins && g_nent < MAX_ENTRIES; j++) {
        if (find_entry(g_wins[j].key)) continue;
        Entry *e = &g_entries[g_nent++]; memset(e, 0, sizeof *e);
        lstrcpyW(e->key, g_wins[j].key); lstrcpyW(e->path, g_wins[j].exe);
        for (int k = 0; k < g_nwins && e->ntasks < MAX_TASKS; k++)
            if (!wcscmp(g_wins[k].key, e->key)) e->tasks[e->ntasks++] = g_wins[k].h;
        changed = 1;
    }
    int k = 0;
    for (int i = 0; i < g_nent; i++) if (g_entries[i].pinned || g_entries[i].ntasks) { if (k != i) g_entries[k] = g_entries[i]; k++; }
    if (k != g_nent) { g_nent = k; changed = 1; }
    DWORD now = GetTickCount();
    for (int i = 0; i < g_nent; i++) {
        Entry *e = &g_entries[i];
        if (force || !e->hicon || now - e->icon_time > 10000) {
            HICON ic = e->ntasks ? icon_for_window(e->tasks[0], e->path) : icon_for_file(e->path);
            if (ic != e->hicon) { e->hicon = ic; e->has_accent = 0; changed = 1; }
            e->icon_time = now;
        }
        wchar_t title[256];
        if (e->ntasks) GetWindowTextW(e->tasks[0], title, 256); else display_name(e->path, title, 256);
        if (wcscmp(title, e->title)) { lstrcpyW(e->title, title); changed = 1; }
    }
    for (int i = 0; i < g_nflash; ) {
        int alive = 0; for (int j = 0; j < g_nwins; j++) if (g_wins[j].h == g_flash[i]) { alive = 1; break; }
        if (!alive) g_flash[i] = g_flash[--g_nflash]; else i++;
    }
    if (changed) bar_invalidate();
}

/* ---------------------------------------------------------------- layout */
/* Logical space: u runs along the bar, v across it. Vertical bars swap the axes at the end. */
static int ovr(int v) { return v > 0 ? px(v) : 0; }
static RECT to_client(int vert, int u1, int v1, int u2, int v2) { return vert ? R(v1, u1, v2, u2) : R(u1, v1, u2, v2); }
static int clock_lines(void) { const char *f = cfg_str("clock_format"); int n = 1; for (; *f; f++) if (*f == '\n') n++; return n; }

static void do_layout(int W, int H, Layout *L) {
    int vert = bar_vertical(), Lg = vert ? H : W, T = vert ? W : H, n = g_nent;
    const char *style = cfg_str("start_style"), *align = cfg_str("alignment");
    int rev = cfg_bool("bar_mirror") || !strcmp(align, "right"), center = !rev && !strcmp(align, "center");
    int a[E_COUNT], c[E_COUNT], i; memset(a, 0, sizeof a); memset(c, 0, sizeof c); memset(L, 0, sizeof *L);
    /* start button */
    int wide = !vert && (!strcmp(style, "classic") || !strcmp(style, "xp"));
    int auto_a = wide ? (int)(T * 2.6) : (vert ? T : (int)(T * 1.2)), ow = ovr(cfg_int("start_btn_w")), oh = ovr(cfg_int("start_btn_h"));
    if (el_show(E_START)) { a[E_START] = vert ? (oh ? oh : auto_a) : (ow ? ow : auto_a); c[E_START] = vert ? (ow ? ow : T) : (oh ? oh : T); if (c[E_START] > T) c[E_START] = T; }
    if (cfg_bool("search_box") && el_show(E_SEARCH)) { a[E_SEARCH] = vert ? T : px(cfg_int("search_w")); c[E_SEARCH] = T; }
    if (el_show(E_DESK)) { a[E_DESK] = px(cfg_int("desk_w")); c[E_DESK] = T; }
    if (cfg_bool("show_preview_button") && el_show(E_PREVIEW)) { a[E_PREVIEW] = vert ? (int)(T * 0.8) : px(34); c[E_PREVIEW] = T; }
    if (el_show(E_CLOCK)) { a[E_CLOCK] = vert ? clock_lines() * px(cfg_int("font_size") + 3) + px(10) : px(cfg_int("clock_w")); c[E_CLOCK] = T; }
    if (cfg_bool("tray_icons") && el_show(E_TRAY) && tray_width() > 0) { a[E_TRAY] = tray_width(); c[E_TRAY] = T; }
    /* system group, packed from the far end */
    static const int sys[] = { E_DESK, E_PREVIEW, E_CLOCK, E_TRAY };
    int q = 0;
    for (int k = 0; k < 4; k++) { int e = sys[k]; if (!a[e]) continue; int u1 = rev ? q : Lg - q - a[e]; L->r[e] = R(u1, (T - c[e]) / 2, u1 + a[e], (T - c[e]) / 2 + c[e]); q += a[e]; }
    int sys_total = q;
    /* main group: start, search, apps */
    int icon_w = vert ? T : (int)(T * 1.15), wanted = (!vert && cfg_bool("show_titles")) ? (icon_w > px(cfg_int("button_width")) ? icon_w : px(cfg_int("button_width"))) : icon_w;
    int widths[MAX_ENTRIES], total = 0; if (!el_show(E_APPS)) n = 0;
    for (i = 0; i < n; i++) { widths[i] = g_entries[i].ntasks ? wanted : icon_w; total += widths[i]; }
    int fixed = a[E_START] + a[E_SEARCH], avail = Lg - sys_total - fixed; if (avail < 0) avail = 0;
    if (n && total > avail) { double f = (double)avail / total; for (i = 0; i < n; i++) { int w = (int)(widths[i] * f); widths[i] = w > px(28) ? w : px(28); } }
    int sum = 0; for (i = 0; i < n; i++) sum += widths[i];
    int p = 0; if (center) { p = (Lg - sys_total - (fixed + sum)) / 2; if (p < 0) p = 0; }
    static const int mainel[] = { E_START, E_SEARCH };
    for (int k = 0; k < 2; k++) { int e = mainel[k]; if (!a[e]) continue; int u1 = rev ? Lg - p - a[e] : p; L->r[e] = R(u1, (T - c[e]) / 2, u1 + a[e], (T - c[e]) / 2 + c[e]); p += a[e]; }
    for (i = 0; i < n; i++) { int u1 = rev ? Lg - p - widths[i] : p; L->ent[i] = R(u1, 0, u1 + widths[i], T); p += widths[i]; }
    /* user drag offsets (relative, so they adapt to bar size) then axes swap */
    for (int e = E_START; e <= E_TRAY; e++) {
        if (!a[e]) continue; int dx, dy; el_offset(e, &dx, &dy); RECT r = L->r[e];
        L->r[e] = to_client(vert, r.left + px(dx), r.top + px(dy), r.right + px(dx), r.bottom + px(dy));
    }
    int dx, dy; el_offset(E_APPS, &dx, &dy);
    for (i = 0; i < n; i++) { RECT r = L->ent[i]; L->ent[i] = to_client(vert, r.left + px(dx), r.top + px(dy), r.right + px(dx), r.bottom + px(dy)); }
}

static int hit(int x, int y, int *idx) {
    int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); *idx = -1;
    static const struct { int e, kind; } order[] = { { E_START, H_START }, { E_SEARCH, H_SEARCH }, { E_DESK, H_DESK }, { E_PREVIEW, H_CLOSEALL }, { E_TRAY, H_TRAY }, { E_CLOCK, H_CLOCK } };
    for (int k = 0; k < 6; k++) if (L.r[order[k].e].right > L.r[order[k].e].left && in_rect(&L.r[order[k].e], x, y)) return order[k].kind;
    if (el_show(E_APPS)) for (int i = 0; i < g_nent; i++) if (in_rect(&L.ent[i], x, y)) { *idx = i; return H_ENTRY; }
    return H_BAR;
}
static int kind_element(int kind) {
    switch (kind) { case H_START: return E_START; case H_SEARCH: return E_SEARCH; case H_DESK: return E_DESK; case H_CLOSEALL: return E_PREVIEW;
        case H_TRAY: return E_TRAY; case H_CLOCK: return E_CLOCK; case H_ENTRY: return E_APPS; default: return -1; }
}
int bar_start_left(void) { int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); RECT b = bar_rect(); return b.left + L.r[E_START].left; }
int bar_start_right(void) { int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); RECT b = bar_rect(); return b.left + L.r[E_START].right; }
int bar_is_hidden(void) { return g_bar_hidden; }

/* ------------------------------------------------------------- painting */
static void hl(HDC dc, RECT r, const Theme *th, int kind /*0 hover 1 active 2 flash*/) {
    if (cfg_bool("glass") && kind != 2) {
        int dark = app_is_dark(); int alpha = kind == 0 ? (dark ? 40 : 30) : (dark ? 70 : 55);
        g_alpha(dc, r, dark ? RGB(255, 255, 255) : RGB(0, 0, 0), alpha);
    } else g_fill(dc, r, kind == 0 ? th->hover : kind == 1 ? th->active : th->flash);
}

static void panes(HDC dc, int lx, int ly, int s, int gap) {
    static const COLORREF col[4] = { RGB(240, 80, 40), RGB(110, 190, 40), RGB(40, 140, 230), RGB(250, 200, 30) };
    int pane = s / 2 - 1; if (pane < 3) pane = 3;
    for (int i = 0; i < 4; i++) g_fill(dc, R(lx + (i % 2) * (pane + gap), ly + (i / 2) * (pane + gap), lx + (i % 2) * (pane + gap) + pane, ly + (i / 2) * (pane + gap) + pane), col[i]);
}

static void draw_start(HDC dc, RECT r, const Theme *th, int hot) {
    const char *st = cfg_str("start_style"); int w = rw(&r), h = r.bottom - r.top, cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    int has_icon = el_icon(E_START)[0] != 0, vert = bar_vertical();
    if (!has_icon && !vert && !strcmp(st, "classic")) {
        RECT b = R(r.left + 2, r.top + 3, r.right - 2, r.bottom - 3); int down = g_start_visible;
        g_fill(dc, b, th->bg);
        COLORREF light = down ? RGB(80, 80, 80) : RGB(255, 255, 255), dark = down ? RGB(255, 255, 255) : RGB(64, 64, 64);
        g_fill(dc, R(b.left, b.top, b.right, b.top + 1), light); g_fill(dc, R(b.left, b.top, b.left + 1, b.bottom), light);
        g_fill(dc, R(b.left, b.bottom - 1, b.right, b.bottom), dark); g_fill(dc, R(b.right - 1, b.top, b.right, b.bottom), dark);
        int s = (int)((b.bottom - b.top) * 0.55), lx = b.left + px(10), ly = (b.top + b.bottom) / 2 - s / 2; panes(dc, lx, ly, s, 2);
        HGDIOBJ o = SelectObject(dc, g_font_big); g_text(dc, L"Start", R(lx + s + px(10), b.top, b.right, b.bottom), DT_VCENTER | DT_SINGLELINE, th->text); SelectObject(dc, o); return;
    }
    if (!has_icon && !vert && !strcmp(st, "xp")) {
        RECT b = R(r.left, r.top + 1, r.right, r.bottom - 1); COLORREF fill = hot ? RGB(90, 185, 90) : RGB(70, 160, 70);
        HBRUSH br = CreateSolidBrush(fill); HPEN pen = CreatePen(PS_SOLID, 1, RGB(24, 90, 24)); HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
        RoundRect(dc, b.left, b.top, b.right, b.bottom, px(24), px(24)); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pen);
        g_fill(dc, R(b.left + 8, b.top + 3, b.right - 8, b.top + 5), RGB(130, 210, 130));
        int s = (int)(h * 0.5), lx = b.left + px(14); panes(dc, lx, cy - s / 2, s, 2);
        HGDIOBJ o = SelectObject(dc, g_font_big); g_text(dc, L"start", R(lx + s + px(10), b.top, b.right, b.bottom), DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255)); SelectObject(dc, o); return;
    }
    if (!has_icon && !strcmp(st, "orb")) {
        int rad = (int)((w < h ? w : h) * 0.46); COLORREF base = (hot || g_start_visible) ? RGB(60, 130, 210) : RGB(30, 90, 170);
        HBRUSH br = CreateSolidBrush(base); HPEN pen = CreatePen(PS_SOLID, 1, RGB(10, 40, 90)); HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
        Ellipse(dc, cx - rad, cy - rad, cx + rad, cy + rad); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pen);
        HBRUSH hb = CreateSolidBrush(RGB(120, 175, 235)); ob = SelectObject(dc, hb); op = SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, cx - rad + 3, cy - rad + 2, cx + rad - 3, cy); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(hb);
        int pane = rad / 2 - 1; if (pane < 3) pane = 3; for (int i = 0; i < 4; i++) g_fill(dc, R(cx + (i % 2 ? 1 : -pane - 1), cy + (i / 2 ? 1 : -pane - 1), cx + (i % 2 ? 1 : -pane - 1) + pane, cy + (i / 2 ? 1 : -pane - 1) + pane), RGB(255, 255, 255));
        return;
    }
    if (hot || g_start_visible) hl(dc, r, th, 0);
    int s = (int)((w < h ? w : h) * 0.56 * cfg_int("start_icon_scale") / 100.0); if (s < 8) s = 8;
    RECT ir = R(cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2);
    COLORREF def = cfg_str("start_color")[0] ? c_hex(cfg_str("start_color"), th->text) : (hot ? th->accent : th->text);
    el_draw(dc, E_START, ir, def, (!strcmp(st, "winlogo") || !strcmp(st, "logo")) ? G_WINLOGO : G_NINEBOX);
}

static void draw_search(HDC dc, RECT r, const Theme *th, int hot) {
    int vert = bar_vertical(); COLORREF ic = el_color(E_SEARCH, th->dim); (void)ic;
    if (vert) { if (hot) hl(dc, r, th, 0); int s = (r.right - r.left) * 6 / 10; el_draw(dc, E_SEARCH, R((r.left + r.right) / 2 - s / 2, (r.top + r.bottom) / 2 - s / 2, (r.left + r.right) / 2 + s / 2, (r.top + r.bottom) / 2 + s / 2), th->text, G_SEARCH); return; }
    int pad = px(6); RECT b = R(r.left + px(2), r.top + pad, r.right - px(2), r.bottom - pad);
    if (cfg_bool("glass")) g_alpha(dc, b, th->input, 215); else g_fill(dc, b, th->input);
    int active = g_start_visible && cfg_bool("start_search_in_bar");
    g_frame(dc, b, (hot || active) ? th->accent : th->active, 1);
    int s = b.bottom - b.top - px(12); if (s < 8) s = 8;
    el_draw(dc, E_SEARCH, R(b.left + px(8), (b.top + b.bottom) / 2 - s / 2, b.left + px(8) + s, (b.top + b.bottom) / 2 + s / 2), th->dim, G_SEARCH);
    const wchar_t *q = start_query(); wchar_t ph[100]; w_from_utf8_buf(cfg_str("search_text"), ph, 100);
    int has = active && q[0];
    HGDIOBJ o = SelectObject(dc, g_font_small);
    g_text(dc, has ? q : ph, R(b.left + px(12) + s, b.top, b.right - px(6), b.bottom), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, has ? th->text : th->dim);
    SelectObject(dc, o);
}

static void draw_icon_shaped(HDC dc, const Entry *e, int x, int y, int size, const Theme *th) {
    const char *shape = cfg_str("icon_shape"); int pad = px(2); HRGN rgn = NULL; RECT rr = { 0, 0, 0, 0 };
    rr = R(x - pad, y - pad, x + size + pad, y + size + pad);
    if (!strcmp(shape, "circle")) rgn = CreateEllipticRgn(rr.left, rr.top, rr.right, rr.bottom);
    else if (!strcmp(shape, "rounded")) { int cr = size / 4 > 4 ? size / 4 : 4; rgn = CreateRoundRectRgn(rr.left, rr.top, rr.right + 1, rr.bottom + 1, cr, cr); }
    else if (!strcmp(shape, "rect") || !strcmp(shape, "square")) rgn = CreateRectRgn(rr.left, rr.top, rr.right, rr.bottom);
    if (rgn) {
        COLORREF bc = cfg_str("icon_border_color")[0] ? c_hex(cfg_str("icon_border_color"), th->accent) : th->accent;
        if (cfg_bool("icon_border")) g_frame(dc, rr, bc, px(cfg_int("icon_border_width")));
        SelectClipRgn(dc, rgn); if (e->hicon) DrawIconEx(dc, x, y, e->hicon, size, size, 0, NULL, DI_NORMAL); SelectClipRgn(dc, NULL); DeleteObject(rgn);
    } else if (e->hicon) DrawIconEx(dc, x, y, e->hicon, size, size, 0, NULL, DI_NORMAL);
}

static void draw_entry(HDC dc, Entry *e, RECT r, int isz, const Theme *th, int lifted) {
    int active = entry_active(e), flashing = 0, vert = bar_vertical();
    for (int i = 0; i < e->ntasks; i++) if (is_flashing(e->tasks[i])) flashing = 1;
    if (lifted) hl(dc, r, th, 1); else if (flashing) hl(dc, r, th, 2); else if (active) hl(dc, r, th, 1);
    else if (g_hover == H_ENTRY && !wcscmp(g_hover_key, e->key)) hl(dc, r, th, 0);
    int bw = rw(&r), bh = r.bottom - r.top;
    int show_text = !vert && cfg_bool("show_titles") && e->ntasks && bw > isz + px(40);
    int ix = r.left + (show_text ? px(10) : (bw - isz) / 2), iy = r.top + (bh - isz) / 2;
    draw_icon_shaped(dc, e, ix, iy, isz, th);
    if (show_text) g_text(dc, e->title, R(ix + isz + px(8), r.top, r.right - px(6), r.bottom), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th->text);
    if (!e->ntasks) return;
    if (!e->has_accent) { e->accent = cfg_bool("icon_accent") ? icon_accent(e->hicon, th->accent) : th->accent; e->has_accent = 1; }
    int thick = px(active ? 3 : 2);
    if (!vert) { int u0 = active ? r.left + 2 : r.left + bw / 4, u1 = active ? r.right - 2 : r.right - bw / 4;
        g_fill(dc, R(u0, r.bottom - thick, u1, r.bottom), e->accent); if (e->ntasks > 1) g_fill(dc, R(u0 + 3, r.bottom - thick - 3, u1 - 3, r.bottom - thick - 2), e->accent); }
    else { int left_bar = !strcmp(cfg_str("position"), "left"); int v0 = active ? r.top + 2 : r.top + bh / 4, v1 = active ? r.bottom - 2 : r.bottom - bh / 4;
        if (left_bar) g_fill(dc, R(r.left, v0, r.left + thick, v1), e->accent); else g_fill(dc, R(r.right - thick, v0, r.right, v1), e->accent); }
}

static void draw_clock(HDC dc, RECT r, const Theme *th) {
    format_clock(g_clock_text, 64);
    if (!bar_vertical()) {
        RECT calc = r; DrawTextW(dc, g_clock_text, -1, &calc, DT_CALCRECT | DT_CENTER | DT_NOPREFIX);
        int th_ = calc.bottom - calc.top, top = r.top + ((r.bottom - r.top - th_) / 2 > 0 ? (r.bottom - r.top - th_) / 2 : 0);
        g_text(dc, g_clock_text, R(r.left, top, r.right, top + th_), DT_CENTER | DT_NOPREFIX, th->text);
        return;
    }
    wchar_t line[64]; int lh = px(cfg_int("font_size") + 3), y = r.top + px(5); const wchar_t *p = g_clock_text; HGDIOBJ o = SelectObject(dc, g_font_small);
    while (*p) { int k = 0; while (*p && *p != L'\n' && k < 63) line[k++] = *p++; line[k] = 0; if (*p == L'\n') p++;
        g_text(dc, line, R(r.left, y, r.right, y + lh), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, th->text); y += lh; }
    SelectObject(dc, o);
}

static void draw(HDC dc, int W, int H) {
    Theme th = app_theme(1); Layout L; do_layout(W, H, &L); int vert = bar_vertical();
    if (cfg_bool("glass")) g_glass_blit(dc, 0, W, H, th.bg); else g_fill(dc, R(0, 0, W, H), th.bg);
    SetBkMode(dc, TRANSPARENT); HGDIOBJ old_font = SelectObject(dc, g_font);
    if (!strcmp(cfg_str("bar_style"), "raised")) { g_fill(dc, R(0, 0, W, 1), RGB(223, 223, 223)); g_fill(dc, R(0, 1, W, 2), RGB(255, 255, 255)); }
    if (L.r[E_START].right > L.r[E_START].left) draw_start(dc, L.r[E_START], &th, g_hover == H_START);
    if (L.r[E_SEARCH].right > L.r[E_SEARCH].left) draw_search(dc, L.r[E_SEARCH], &th, g_hover == H_SEARCH);
    int isz = px(cfg_int("icon_size")), dragged = -1; int n = el_show(E_APPS) ? g_nent : 0;
    for (int i = 0; i < n; i++) { if (g_drag && !wcscmp(g_entries[i].key, g_drag_key)) { dragged = i; continue; } draw_entry(dc, &g_entries[i], L.ent[i], isz, &th, 0); }
    if (dragged >= 0) { RECT r = L.ent[dragged]; int sh = vert ? g_drag_x - g_drag_dx : 0, shx = vert ? 0 : g_drag_x - g_drag_dx;
        RECT d = vert ? R(r.left, sh, r.right, sh + (r.bottom - r.top)) : R(shx, r.top, shx + rw(&r), r.bottom); draw_entry(dc, &g_entries[dragged], d, isz, &th, 1); }
    if (cfg_bool("show_separator")) {
        int tw = px(2);
        for (int i = 1; i < n; i++) if (g_entries[i - 1].pinned && !g_entries[i].pinned) {
            if (vert) { int sy = L.ent[i].top; g_alpha(dc, R((int)(W * 0.2), sy - tw / 2, (int)(W * 0.8), sy - tw / 2 + tw), th.text, 130); }
            else { int sx = L.ent[i].left; g_alpha(dc, R(sx - tw / 2, (int)(H * 0.2), sx - tw / 2 + tw, (int)(H * 0.8)), th.text, 130); }
            break; }
    }
    if (L.r[E_CLOCK].right > L.r[E_CLOCK].left) draw_clock(dc, L.r[E_CLOCK], &th);
    if (L.r[E_PREVIEW].right > L.r[E_PREVIEW].left) { RECT r = L.r[E_PREVIEW]; if (g_hover == H_CLOSEALL) hl(dc, r, &th, 0); int s = (r.right - r.left < r.bottom - r.top ? r.right - r.left : r.bottom - r.top) * 55 / 100;
        el_draw(dc, E_PREVIEW, R((r.left + r.right) / 2 - s / 2, (r.top + r.bottom) / 2 - s / 2, (r.left + r.right) / 2 + s / 2, (r.top + r.bottom) / 2 + s / 2), th.text, G_PREVIEW); }
    if (L.r[E_TRAY].right > L.r[E_TRAY].left) tray_draw(dc, L.r[E_TRAY], vert);
    if (L.r[E_DESK].right > L.r[E_DESK].left) { RECT r = L.r[E_DESK]; if (g_hover == H_DESK) hl(dc, r, &th, 0);
        if (vert) g_fill(dc, R(4, r.top, W - 4, r.top + 1), th.active); else g_fill(dc, R(r.left, 4, r.left + 1, H - 4), th.active); }
    if (g_edit_layout == 1) {
        for (int e = E_START; e <= E_TRAY; e++) if (L.r[e].right > L.r[e].left) { g_alpha(dc, L.r[e], th.accent, 40); g_frame(dc, L.r[e], th.accent, 1); }
        if (n) { RECT u = L.ent[0]; for (int i = 1; i < n; i++) { if (L.ent[i].left < u.left) u.left = L.ent[i].left; if (L.ent[i].top < u.top) u.top = L.ent[i].top; if (L.ent[i].right > u.right) u.right = L.ent[i].right; if (L.ent[i].bottom > u.bottom) u.bottom = L.ent[i].bottom; }
            g_alpha(dc, u, th.accent, 25); g_frame(dc, u, th.accent, 1); }
    }
    SelectObject(dc, old_font);
}

static void paint(void) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(g_bar_hwnd, &ps); int W, H; client(&W, &H);
    if (W > 0 && H > 0) { HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H); HGDIOBJ old = SelectObject(mem, bmp);
        draw(mem, W, H); BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); }
    EndPaint(g_bar_hwnd, &ps);
}

/* ------------------------------------------------------------ interaction */
static int unpx(int v) { double sc = cfg_num("scale"); if (sc <= 0) sc = 1.0; return (int)(v / sc); }

static int popup(const wchar_t **lab, const int *id, const int *fl, int n) {
    RECT b = bar_rect(); POINT pt; GetCursorPos(&pt); int x = pt.x, y = pt.y; UINT f = TPM_RETURNCMD | TPM_RIGHTBUTTON; const char *pos = cfg_str("position");
    if (!strcmp(pos, "top")) y = b.bottom; else if (!strcmp(pos, "left")) x = b.right; else if (!strcmp(pos, "right")) { x = b.left; f |= TPM_RIGHTALIGN; }
    else { f |= TPM_BOTTOMALIGN; y = b.top; }
    return app_popup(g_bar_hwnd, lab, id, fl, n, f, x, y);
}

static void toggle_window(HWND h) {
    if (IsIconic(h)) { ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); }
    else if (is_active_hwnd(h)) ShowWindow(h, SW_MINIMIZE);
    else SetForegroundWindow(h);
    flash_del(h); bar_invalidate();
}

static void window_list_menu(const Entry *only) {
    static wchar_t titles[MAX_WINS][64]; static HWND hw[MAX_WINS];
    const wchar_t *lab[MAX_WINS]; int id[MAX_WINS], fl[MAX_WINS], n = 0;
    for (int i = 0; i < g_nent; i++) {
        if (only && only != &g_entries[i]) continue;
        for (int j = 0; j < g_entries[i].ntasks && n < MAX_WINS; j++) {
            wchar_t t[256]; GetWindowTextW(g_entries[i].tasks[j], t, 256); if (!t[0]) lstrcpyW(t, L"(window)");
            lstrcpynW(titles[n], t, 60); hw[n] = g_entries[i].tasks[j]; lab[n] = titles[n]; id[n] = 3000 + n; fl[n] = 0; n++;
        }
    }
    if (!n) return;
    int cmd = popup(lab, id, fl, n);
    if (cmd >= 3000 && cmd - 3000 < n) toggle_window(hw[cmd - 3000]);
}

static void click_entry(Entry *e) {
    if (!e->ntasks) app_run(e->path, NULL, SW_SHOWNORMAL);
    else if (e->ntasks == 1) toggle_window(e->tasks[0]);
    else window_list_menu(e);
}

static void entry_menu(Entry *e) {
    HWND h = primary(e); int topmost = h && (GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TOPMOST);
    const wchar_t *lab[16]; int id[16], fl[16], n = 0;
    #define ADD(i, t, f) do { lab[n] = (t); id[n] = (i); fl[n] = (f); n++; } while (0)
    wchar_t head[48]; lstrcpynW(head, e->title[0] ? e->title : e->key, 40);
    ADD(0, head, MF_GRAYED); ADD(-1, NULL, 0); ADD(1, L"Open new window", 0);
    if (h) { ADD(2, L"Restore", 0); ADD(3, L"Minimize", 0); ADD(4, L"Maximize", 0); ADD(5, L"Close window", 0); }
    if (e->ntasks > 1) ADD(6, L"Close all windows", 0);
    if (h) { ADD(-1, NULL, 0); ADD(7, L"Always on top", topmost ? MF_CHECKED : 0); }
    ADD(-1, NULL, 0); ADD(8, e->pinned ? L"Unpin from taskbar" : L"Pin to taskbar", 0); ADD(9, L"Open file location", 0);
    if (h) ADD(10, L"End task (force quit)", 0);
    wchar_t key[MAX_PATH], path[MAX_PATH]; lstrcpyW(key, e->key); lstrcpyW(path, e->path);
    int pinned = e->pinned; HWND tasks[MAX_TASKS]; int nt = e->ntasks; memcpy(tasks, e->tasks, sizeof(HWND) * (size_t)nt);
    int cmd = popup(lab, id, fl, n);
    #undef ADD
    switch (cmd) {
    case 1: app_run(path, NULL, SW_SHOWNORMAL); break;
    case 2: if (h) { ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); } break;
    case 3: if (h) ShowWindow(h, SW_MINIMIZE); break;
    case 4: if (h) { ShowWindow(h, SW_MAXIMIZE); SetForegroundWindow(h); } break;
    case 5: if (h) anim_window(h, 1); break;
    case 6: for (int i = 0; i < nt; i++) anim_window(tasks[i], 1); break;
    case 7: if (h) SetWindowPos(h, topmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); break;
    case 8: if (pinned) bar_unpin(key); else bar_pin(path); break;
    case 9: { wchar_t ex[MAX_PATH], prm[MAX_PATH + 16]; exe_for(path, ex, MAX_PATH); _snwprintf(prm, MAX_PATH + 16, L"/select,\"%ls\"", ex); app_run(L"explorer.exe", prm, SW_SHOWNORMAL); break; }
    case 10: if (h) { DWORD pid = 0; wchar_t prm[64]; GetWindowThreadProcessId(h, &pid); _snwprintf(prm, 64, L"/F /PID %lu", (unsigned long)pid); app_run(L"taskkill.exe", prm, SW_HIDE); } break;
    }
}

void bar_show_desktop(void) {
    if (g_nhidden) { for (int i = 0; i < g_nhidden; i++) ShowWindow(g_hidden[i], SW_RESTORE); g_nhidden = 0; }
    else for (int i = 0; i < g_nent; i++) for (int j = 0; j < g_entries[i].ntasks; j++) {
        HWND h = g_entries[i].tasks[j]; if (!IsIconic(h) && g_nhidden < MAX_WINS) { ShowWindow(h, SW_SHOWMINNOACTIVE); g_hidden[g_nhidden++] = h; } }
}

void bar_close_all_windows(void) {
    int count = 0; for (int i = 0; i < g_nent; i++) count += g_entries[i].ntasks;
    if (!count) return;
    wchar_t msg[96]; _snwprintf(msg, 96, L"Close all %d open window%ls?", count, count == 1 ? L"" : L"s");
    if (app_confirm(g_bar_hwnd, L"Close all windows", msg)) for (int i = 0; i < g_nent; i++) for (int j = 0; j < g_entries[i].ntasks; j++) anim_window(g_entries[i].tasks[j], 1);
}

static void bar_menu(void) {
    const wchar_t *lab[] = { L"Show desktop", L"Close all windows", NULL, L"Taskbar settings", L"Edit layout (drag elements)", L"Settings", L"Reload settings", NULL, L"Exit taskbar" };
    int id[] = { 1, 2, -1, 3, 4, 5, 6, -1, 7 }, fl[] = { 0, 0, 0, 0, g_edit_layout == 1 ? MF_CHECKED : 0, 0, 0, 0, 0 };
    int cmd = popup(lab, id, fl, 9);
    if (cmd == 1) bar_show_desktop(); else if (cmd == 2) bar_close_all_windows();
    else if (cmd == 3) settings_open_page("Taskbar"); else if (cmd == 4) { g_edit_layout = g_edit_layout == 1 ? 0 : 1; bar_invalidate(); }
    else if (cmd == 5) settings_open(-1); else if (cmd == 6) app_reload(); else if (cmd == 7) PostMessageW(g_bar_hwnd, WM_CLOSE, 0, 0);
}

static void edit_menu(int e) {
    const wchar_t *lab[3]; int id[3] = { 1, 2, 3 }, fl[3] = { 0, 0, 0 }; wchar_t t[80]; wchar_t l[60]; w_from_utf8_buf(el_label(e), l, 60);
    _snwprintf(t, 80, L"Hide: %ls", l); lab[0] = t; lab[1] = L"Reset position"; lab[2] = L"Done editing";
    int cmd = popup(lab, id, fl, 3);
    if (cmd == 1) { el_set_show(e, 0); app_save(); bar_invalidate(); } else if (cmd == 2) { el_set_offset(e, 0, 0); app_save(); bar_invalidate(); }
    else if (cmd == 3) { g_edit_layout = 0; bar_invalidate(); }
}

static void reorder_drag(int pos) {
    int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); int vert = bar_vertical();
    int di = entry_index(g_drag_key); if (di < 0) return;
    int len = vert ? L.ent[di].bottom - L.ent[di].top : rw(&L.ent[di]), center = pos - g_drag_dx + len / 2, to = 0;
    for (int i = 0; i < g_nent; i++) if (i != di) { int c = vert ? (L.ent[i].top + L.ent[i].bottom) / 2 : (L.ent[i].left + L.ent[i].right) / 2; if (c < center) to++; }
    if (to == di) return;
    Entry tmp = g_entries[di];
    if (to < di) memmove(&g_entries[to + 1], &g_entries[to], sizeof(Entry) * (size_t)(di - to)); else memmove(&g_entries[di], &g_entries[di + 1], sizeof(Entry) * (size_t)(to - di));
    g_entries[to] = tmp;
}

static void finish_drag(void) {
    g_drag = 0; const char *paths[MAX_ENTRIES]; char *own[MAX_ENTRIES]; int n = 0;
    for (int i = 0; i < g_nent; i++) if (g_entries[i].pinned) { own[n] = utf8_from_w(g_entries[i].path); paths[n] = own[n]; n++; }
    cfg_list_set("pinned", paths, n); for (int i = 0; i < n; i++) free(own[i]);
    app_save(); bar_invalidate();
}

/* ------------------------------------------------ auto-hide / reveal logic */
static void force_front(int hard) {
    if (!g_bar_hwnd || g_bar_hidden) return;
    UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    if (hard) SetWindowPos(g_bar_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, f);
    SetWindowPos(g_bar_hwnd, HWND_TOPMOST, 0, 0, 0, 0, f);
}

static const char *slide_dir(void) { const char *d = cfg_str("anim_bar_dir"); return !strcmp(d, "auto") || !d[0] ? anim_edge_dir(cfg_str("position")) : d; }
static int slide_dist(void) { RECT b = bar_rect(); int t = bar_vertical() ? b.right - b.left : b.bottom - b.top; return t + px(cfg_int("bar_gap")) + px(6); }

static void bar_set_hidden(int hide) {
    g_bar_hidden = hide; int anim = cfg_bool("animations") && cfg_bool("anim_bar_on");
    if (hide) {
        if (anim) anim_run(g_bar_hwnd, g_final, anim_offset(g_final, slide_dir(), slide_dist()), cfg_int("anim_bar_ms"), -1, -1, ANIM_END_HIDE, g_final);
        else ShowWindow(g_bar_hwnd, SW_HIDE);
    } else {
        anim_cancel(g_bar_hwnd);
        if (anim) { RECT from = anim_offset(g_final, slide_dir(), slide_dist());
            SetWindowPos(g_bar_hwnd, HWND_TOPMOST, from.left, from.top, from.right - from.left, from.bottom - from.top, SWP_NOACTIVATE);
            ShowWindow(g_bar_hwnd, SW_SHOWNA); anim_run(g_bar_hwnd, from, g_final, cfg_int("anim_bar_ms"), -1, -1, ANIM_END_NONE, g_final); }
        else { SetWindowPos(g_bar_hwnd, HWND_TOPMOST, g_final.left, g_final.top, g_final.right - g_final.left, g_final.bottom - g_final.top, SWP_NOACTIVATE); ShowWindow(g_bar_hwnd, SW_SHOWNA); }
        force_front(1); bar_invalidate();
    }
}

void bar_reveal(int seconds) {
    g_reveal_len = seconds ? seconds : cfg_int("reveal_seconds"); g_reveal_until = GetTickCount() + (DWORD)g_reveal_len * 1000;
    if (g_bar_hidden) bar_set_hidden(0);
}

static int cursor_in_bar(int grow) {
    POINT pt; GetCursorPos(&pt); RECT b = bar_rect();
    return pt.x >= b.left - grow && pt.x <= b.right + grow && pt.y >= b.top - grow && pt.y <= b.bottom + grow;
}
static int edge_touch(void) {
    POINT pt; GetCursorPos(&pt); int sw, sh; screen_size(&sw, &sh); const char *p = cfg_str("position");
    if (!strcmp(p, "left")) return pt.x <= 0; if (!strcmp(p, "right")) return pt.x >= sw - 1; if (!strcmp(p, "top")) return pt.y <= 0; return pt.y >= sh - 1;
}
static int fg_info(HWND *out) {                      /* 1 when a normal program window is maximised */
    HWND fg = GetForegroundWindow(); *out = fg;
    if (!fg || fg == g_bar_hwnd || !IsWindowVisible(fg) || IsIconic(fg)) return 0;
    wchar_t cls[128]; GetClassNameW(fg, cls, 128);
    if (is_ignored_class(cls) || !wcsncmp(cls, L"WinShell", 8)) return 0;
    return IsZoomed(fg) ? 1 : 0;
}
int bar_fg_excluded(void) {
    HWND fg = GetForegroundWindow(); JNode *ex = cfg_node("strip_exclude"); if (!fg || !ex || ex->t != J_ARR || !ex->child) return 0;
    wchar_t pe[MAX_PATH], key[MAX_PATH]; window_process_path(fg, pe, MAX_PATH); if (!pe[0]) return 0; app_key(pe, key, MAX_PATH);
    for (JNode *c = ex->child; c; c = c->next) { wchar_t p[MAX_PATH], pe2[MAX_PATH], k2[MAX_PATH]; w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe2, MAX_PATH); app_key(pe2, k2, MAX_PATH); if (!wcscmp(key, k2)) return 1; }
    return 0;
}

static void check_hide(void) {
    HWND fg; int zoomed = fg_info(&fg); DWORD now = GetTickCount();
    int hide = cfg_bool("bar_autohide") || (cfg_bool("strip_maximized") && zoomed);
    if (hide && (int)(g_reveal_until - now) > 0) {
        hide = 0; if (g_menu_depth > 0 || g_start_visible || cursor_in_bar(6) || g_edit_layout) g_reveal_until = now + (DWORD)g_reveal_len * 1000;
    }
    if (g_edit_layout == 1) hide = 0;
    if (hide != g_bar_hidden) bar_set_hidden(hide);
    reveal_show(g_bar_hidden && cfg_bool("strip_enabled") && !bar_fg_excluded());
}

static void fast_tick(void) {
    g_fast++;
    if (!g_bar_hwnd) return;
    if (g_fast % 4 == 0 && tray_refresh()) bar_invalidate();
    DWORD now = GetTickCount();
    for (int i = 0; i < g_npending; ) {                      /* animate newly opened program windows */
        HWND h = g_pending[i]; int drop = !IsWindow(h) || now - g_pending_t[i] > 3000;
        if (!drop && IsWindowVisible(h) && is_task_window(h)) { wchar_t cls[64]; GetClassNameW(h, cls, 64); if (wcsncmp(cls, L"WinShell", 8)) anim_window(h, 0); drop = 1; }
        if (drop) { g_pending[i] = g_pending[--g_npending]; g_pending_t[i] = g_pending_t[g_npending]; } else i++;
    }
    if (!g_bar_hidden && g_menu_depth == 0 && !anim_busy(g_bar_hwnd)) {
        int hard = g_need_hard; g_need_hard = 0;
        if (!hard) { if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && g_fast % 3 == 0 && cursor_in_bar(60)) hard = 1; }
        force_front(hard);
    } else if (g_bar_hidden && g_fast % 4 == 0 && edge_touch()) bar_reveal(0);
    skin_tick();
}

static void on_timer(void) {
    g_tick++; int dirty = 0;
    if (cfg_mtime() != app_config_mtime()) app_reload();
    if (g_tick % 2 == 0) bar_refresh(0);
    check_hide();
    HWND fg = GetForegroundWindow();
    if (fg != g_last_fg) { g_last_fg = fg; dirty = 1; g_need_hard = 1; }
    wchar_t now[64]; format_clock(now, 64); if (wcscmp(now, g_clock_text)) dirty = 1;
    if (dirty) bar_invalidate();
}

/* ------------------------------------------------------------ window proc */
static void on_move(int x, int y, int kind, int idx) {
    int vert = bar_vertical(), along = vert ? y : x;
    if (g_ed_el >= 0) {
        int du = vert ? y - g_ed_sy : x - g_ed_sx, dv = vert ? x - g_ed_sx : y - g_ed_sy; int W, H; client(&W, &H);
        int lim = unpx((vert ? W : H) / 2);
        int ndy = g_ed_dy + unpx(dv); if (ndy > lim) ndy = lim; if (ndy < -lim) ndy = -lim;
        el_set_offset(g_ed_el, g_ed_dx + unpx(du), ndy); bar_invalidate(); return;
    }
    if (g_press && !g_drag && abs(along - g_press_x) > px(6)) {
        int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); int i = entry_index(g_press_key);
        if (i >= 0) { g_drag_dx = along - (vert ? L.ent[i].top : L.ent[i].left); g_drag = 1; lstrcpyW(g_drag_key, g_press_key); }
    }
    if (g_drag) { g_drag_x = along; reorder_drag(along); bar_invalidate(); return; }
    int nk = (kind == H_ENTRY || kind == H_START || kind == H_SEARCH || kind == H_DESK || kind == H_CLOSEALL) ? kind : H_NONE;
    wchar_t nkey[MAX_PATH]; nkey[0] = 0; if (nk == H_ENTRY && idx >= 0) lstrcpyW(nkey, g_entries[idx].key);
    if (!g_tracking) { TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, g_bar_hwnd, 0 }; TrackMouseEvent(&t); g_tracking = 1; }
    if (nk != g_hover || wcscmp(nkey, g_hover_key)) { g_hover = nk; lstrcpyW(g_hover_key, nkey); bar_invalidate(); }
}

static void on_left_up(int kind, int idx) {
    if (g_ed_el >= 0) { g_ed_el = -1; app_save(); return; }
    if (g_press) {
        ReleaseCapture(); wchar_t pk[MAX_PATH]; lstrcpyW(pk, g_press_key); g_press = 0;
        if (g_drag) { finish_drag(); return; }
        if (kind == H_ENTRY && idx >= 0 && !wcscmp(g_entries[idx].key, pk)) click_entry(&g_entries[idx]);
        return;
    }
    if (kind == H_CLOCK) { settings_open_page("Clock and buttons"); return; }
    if (kind == H_START) start_toggle(); else if (kind == H_SEARCH) start_open_search();
    else if (kind == H_DESK) bar_show_desktop(); else if (kind == H_CLOSEALL) window_list_menu(NULL);
}

static LRESULT CALLBACK bar_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_shellhook_msg && msg == g_shellhook_msg) {
        UINT event = (UINT)(wp & 0x7FFF);
        if (wp == 0x8006) flash_add((HWND)lp); else if (event == 4) flash_del((HWND)lp);
        else if (event == 1 && g_npending < 16) { g_pending[g_npending] = (HWND)lp; g_pending_t[g_npending++] = GetTickCount(); }
        g_need_hard = 1; bar_refresh(event == 6); bar_invalidate(); return 0;
    }
    int hd; LRESULT mr;
    switch (msg) {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_MEASUREITEM: case WM_DRAWITEM: mr = app_menu_msg(msg, wp, lp, &hd); if (hd) return mr; break;
    case WM_TIMER: if (wp == 2) fast_tick(); else on_timer(); return 0;
    case WM_COPYDATA: { COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lp; if (cds && cds->dwData == 1 && cds->lpData) app_run(L"explorer.exe", (const wchar_t *)cds->lpData, SW_SHOWNORMAL); return 1; }
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), idx; int kind = hit(x, y, &idx); int vert = bar_vertical();
        if (msg == WM_MOUSEMOVE) on_move(x, y, kind, idx);
        else if (g_edit_layout == 1 && (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONUP)) {
            int e = kind_element(kind);
            if (msg == WM_LBUTTONDOWN && e >= 0) { g_ed_el = e; g_ed_sx = x; g_ed_sy = y; el_offset(e, &g_ed_dx, &g_ed_dy); SetCapture(hwnd); }
            else if (msg == WM_RBUTTONUP) { if (e >= 0) edit_menu(e); else bar_menu(); }
        }
        else if (msg == WM_LBUTTONDOWN) { if (kind == H_ENTRY && idx >= 0) { g_press = 1; g_press_x = vert ? y : x; lstrcpyW(g_press_key, g_entries[idx].key); SetCapture(hwnd); } }
        else if (msg == WM_LBUTTONUP) { if (g_ed_el >= 0) ReleaseCapture(); if (kind == H_TRAY && g_ed_el < 0 && g_edit_layout != 1) tray_click(x, y, 0); else on_left_up(kind, idx); }
        else if (msg == WM_LBUTTONDBLCLK) { if (kind == H_TRAY) tray_click(x, y, 2); }
        else if (msg == WM_RBUTTONUP) {
            if (kind == H_TRAY) tray_click(x, y, 1); else if (kind == H_ENTRY && idx >= 0) entry_menu(&g_entries[idx]); else bar_menu();
        } else if (msg == WM_MBUTTONUP && kind == H_ENTRY && idx >= 0 && g_entries[idx].path[0]) app_run(g_entries[idx].path, NULL, SW_SHOWNORMAL);
        return 0; }
    case WM_MOUSELEAVE: g_tracking = 0; g_hover = H_NONE; g_hover_key[0] = 0; bar_invalidate(); return 0;
    case WM_DISPLAYCHANGE: app_apply_all(); return 0;
    case WM_DESTROY: app_shutdown(); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* --------------------------------------------------------- create / apply */
typedef BOOL (WINAPI *ShellHook_t)(HWND);

void bar_apply(void) {
    if (!g_bar_hwnd) return;
    RECT b = bar_rect(); g_final = b; int w = rw(&b), h = b.bottom - b.top;
    if (!g_bar_hidden && !anim_busy(g_bar_hwnd)) SetWindowPos(g_bar_hwnd, HWND_TOPMOST, b.left, b.top, w, h, SWP_NOACTIVATE);
    if (cfg_bool("round_corners") && cfg_int("corner_radius") > 0 && strcmp(cfg_str("bar_style"), "raised")) { int r = px(cfg_int("corner_radius")) * 2; SetWindowRgn(g_bar_hwnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, r, r), TRUE); }
    else SetWindowRgn(g_bar_hwnd, NULL, TRUE);
    apply_work_area(); apply_glass(g_bar_hwnd, cfg_bool("glass"), cfg_int("glass_opacity"));
    HWND wine = FindWindowW(L"Shell_TrayWnd", NULL);
    if (wine) {
        if (cfg_bool("hide_wine_taskbar") && cfg_bool("tray_icons")) { SetWindowPos(wine, NULL, -32000, -32000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); ShowWindow(wine, SW_SHOWNOACTIVATE); }
        else ShowWindow(wine, cfg_bool("hide_wine_taskbar") ? SW_HIDE : SW_SHOW);
    }
    bar_rebuild_pins(); bar_refresh(1); bar_invalidate();
}

int bar_create(void) {
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS; wc.lpfnWndProc = bar_proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc); g_shellhook_msg = RegisterWindowMessageW(L"SHELLHOOK");
    RECT b = bar_rect(); g_final = b;
    g_bar_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, CLASS_NAME, L"WinShell Taskbar", WS_POPUP, b.left, b.top, rw(&b), b.bottom - b.top, NULL, NULL, wc.hInstance, NULL);
    if (!g_bar_hwnd) return 0;
    SetTimer(g_bar_hwnd, 1, 1000, NULL); SetTimer(g_bar_hwnd, 2, 40, NULL);
    ShellHook_t reg = (ShellHook_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "RegisterShellHookWindow");
    if (reg) reg(g_bar_hwnd);
    bar_apply(); g_bar_hidden = 1; bar_set_hidden(0);      /* slides in when animations are on */
    return 1;
}

RECT bar_start_screen_rect(void) {
    int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); RECT b = bar_rect(), s = L.r[E_START];
    OffsetRect(&s, b.left, b.top); return s;
}
