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

enum { H_NONE, H_START, H_DESK, H_CLOSEALL, H_KBD, H_TRAY, H_BAR, H_ENTRY };

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
static int  g_bar_hidden, g_need_hard, g_fs_force;
static DWORD g_reveal_until; static int g_reveal_len = 5; static int g_reveal_alpha = 255;
static int g_cur_hide_mode;
static UINT g_shellhook_msg;

typedef struct { RECT start, desk, closeall, clock, kbd, tray; RECT ent[MAX_ENTRIES]; } Layout;
static Layout g_last_layout;

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
static void do_layout(int W, int H, Layout *L) {
    int start_w = (int)(H * 1.2), clock_w = px(100), desk_w = px(10);
    int ca_w = cfg_bool("show_preview_button") ? px(34) : 0;
    const char *align = cfg_str("alignment");
    int kw = cfg_bool("kbd_tray_button") ? px(30) : 0;
    int tw = cfg_bool("tray_icons") ? tray_width() : 0;
    int icon_w = (int)(H * 1.15);
    int wanted = cfg_bool("show_titles") ? (icon_w > px(cfg_int("button_width")) ? icon_w : px(cfg_int("button_width"))) : icon_w;
    int widths[MAX_ENTRIES], total = 0, n = g_nent;
    for (int i = 0; i < n; i++) { widths[i] = g_entries[i].ntasks ? wanted : icon_w; total += widths[i]; }
    memset(L, 0, sizeof *L);

    if (!strcmp(align, "right")) {
        L->desk = R(0, 0, desk_w, H);
        L->closeall = R(desk_w, 0, desk_w + ca_w, H);
        L->clock = R(L->closeall.right, 0, L->closeall.right + clock_w, H);
        L->kbd = kw ? R(L->clock.right, 0, L->clock.right + kw, H) : R(0, 0, 0, 0);
        int kr = kw ? L->kbd.right : L->clock.right;
        L->tray = tw ? R(kr, 0, kr + tw, H) : R(0, 0, 0, 0);
        int lim_l = tw ? L->tray.right : kr;
        int avail = W - lim_l - start_w; if (avail < 0) avail = 0;
        if (n && total > avail) { double f = (double)avail / total; for (int i = 0; i < n; i++) { int w = (int)(widths[i] * f); widths[i] = w > px(28) ? w : px(28); } }
        L->start = R(W - start_w, 0, W, H);
        int x = W - start_w;
        for (int i = 0; i < n; i++) { L->ent[i] = R(x - widths[i], 0, x, H); x -= widths[i]; }
        return;
    }
    L->desk = R(W - desk_w, 0, W, H);
    L->closeall = R(L->desk.left - ca_w, 0, L->desk.left, H);
    L->clock = R(L->closeall.left - clock_w, 0, L->closeall.left, H);
    L->kbd = kw ? R(L->clock.left - kw, 0, L->clock.left, H) : R(0, 0, 0, 0);
    int kl = kw ? L->kbd.left : L->clock.left;
    L->tray = tw ? R(kl - tw, 0, kl, H) : R(0, 0, 0, 0);
    int lim = tw ? L->tray.left : kl;
    int avail = lim - start_w; if (avail < 0) avail = 0;
    if (n && total > avail) { double f = (double)avail / total; for (int i = 0; i < n; i++) { int w = (int)(widths[i] * f); widths[i] = w > px(28) ? w : px(28); } }
    int sum = 0; for (int i = 0; i < n; i++) sum += widths[i];
    int group_w = start_w + sum, x0 = 0;
    if (!strcmp(align, "center")) { x0 = (lim - group_w) / 2; if (x0 < 0) x0 = 0; }
    L->start = R(x0, 0, x0 + start_w, H);
    int x = x0 + start_w;
    for (int i = 0; i < n; i++) { L->ent[i] = R(x, 0, x + widths[i], H); x += widths[i]; }
}

int bar_start_left(void) { int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); return bar_rect().left + L.start.left; }
int bar_start_right(void) { int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); return bar_rect().left + L.start.right; }

static int hit(int x, int y, int *idx) {
    int W, H; client(&W, &H); Layout L; do_layout(W, H, &L); *idx = -1;
    if (in_rect(&L.start, x, y)) return H_START;
    for (int i = 0; i < g_nent; i++) if (in_rect(&L.ent[i], x, y)) { *idx = i; return H_ENTRY; }
    if (in_rect(&L.desk, x, y)) return H_DESK;
    if (cfg_bool("show_preview_button") && in_rect(&L.closeall, x, y)) return H_CLOSEALL;
    if (L.kbd.right > L.kbd.left && in_rect(&L.kbd, x, y)) return H_KBD;
    if (L.tray.right > L.tray.left && in_rect(&L.tray, x, y)) return H_TRAY;
    return H_BAR;
}

/* ------------------------------------------------------------- painting */
static void hl(HDC dc, RECT r, const Theme *th, int kind /*0 hover 1 active 2 flash*/) {
    if (cfg_bool("glass") && kind != 2) {
        int dark = app_is_dark();
        int alpha = kind == 0 ? (dark ? 40 : 30) : (dark ? 70 : 55);
        g_alpha(dc, r, dark ? RGB(255, 255, 255) : RGB(0, 0, 0), alpha);
    } else g_fill(dc, r, kind == 0 ? th->hover : kind == 1 ? th->active : th->flash);
}

static void draw_start_glyph(HDC dc, int cx, int cy, int size, const Theme *th, int hot) {
    COLORREF color;
    if (cfg_str("start_color")[0]) color = c_hex(cfg_str("start_color"), th->text);
    else color = hot ? th->accent : th->text;
    int pane = (int)(size * 0.46); if (pane < 3) pane = 3;
    int gap = (int)(size * 0.08); if (gap < 1) gap = 1;
    int total = 2 * pane + gap, sx = cx - total / 2, sy = cy - total / 2;
    for (int dx = 0; dx <= pane + gap; dx += pane + gap)
        for (int dy = 0; dy <= pane + gap; dy += pane + gap)
            g_fill(dc, R(sx + dx, sy + dy, sx + dx + pane, sy + dy + pane), color);
}

static void draw_icon_shaped(HDC dc, const Entry *e, int x, int y, int size, const Theme *th) {
    const char *shape = cfg_str("icon_shape"); int pad = px(2); HRGN rgn = NULL; RECT rr = { 0 };
    if (!strcmp(shape, "circle")) { rr = R(x - pad, y - pad, x + size + pad, y + size + pad); rgn = CreateEllipticRgn(rr.left, rr.top, rr.right, rr.bottom); }
    else if (!strcmp(shape, "round")) { int cr = size / 4 > 4 ? size / 4 : 4; rr = R(x - pad, y - pad, x + size + pad, y + size + pad); rgn = CreateRoundRectRgn(rr.left, rr.top, rr.right, rr.bottom, cr, cr); }
    else if (!strcmp(shape, "rect")) { rr = R(x - pad, y - pad, x + size + pad, y + size + pad); rgn = CreateRectRgn(rr.left, rr.top, rr.right, rr.bottom); }
    if (rgn) {
        if (cfg_bool("icon_shape_border")) g_frame(dc, rr, th->accent, 1);
        SelectClipRgn(dc, rgn);
        if (e->hicon) DrawIconEx(dc, x, y, e->hicon, size, size, 0, NULL, DI_NORMAL);
        SelectClipRgn(dc, NULL); DeleteObject(rgn);
    } else if (e->hicon) DrawIconEx(dc, x, y, e->hicon, size, size, 0, NULL, DI_NORMAL);
}

static void draw_entry(HDC dc, Entry *e, RECT r, int isz, const Theme *th, int H, int lifted) {
    int active = entry_active(e), flashing = 0;
    for (int i = 0; i < e->ntasks; i++) if (is_flashing(e->tasks[i])) flashing = 1;
    if (lifted) hl(dc, r, th, 1);
    else if (flashing) hl(dc, r, th, 2);
    else if (active) hl(dc, r, th, 1);
    else if (g_hover == H_ENTRY && !wcscmp(g_hover_key, e->key)) hl(dc, r, th, 0);
    int bw = rw(&r);
    int show_text = cfg_bool("show_titles") && e->ntasks && bw > isz + px(40);
    int ix = r.left + (show_text ? px(10) : (bw - isz) / 2), iy = r.top + (H - isz) / 2;
    draw_icon_shaped(dc, e, ix, iy, isz, th);
    if (show_text)
        g_text(dc, e->title, R(ix + isz + px(8), r.top, r.right - px(6), r.bottom),
               DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, e->ntasks ? th->text : th->dim);
    if (e->ntasks) {
        if (!e->has_accent) { e->accent = cfg_bool("icon_accent") ? icon_accent(e->hicon, th->accent) : th->accent; e->has_accent = 1; }
        int thick = px(active ? 3 : 2), u0, u1;
        if (active) { u0 = r.left + 2; u1 = r.right - 2; } else { u0 = r.left + bw / 4; u1 = r.right - bw / 4; }
        g_fill(dc, R(u0, r.bottom - thick, u1, r.bottom), e->accent);
        if (e->ntasks > 1) g_fill(dc, R(u0 + 3, r.bottom - thick - 3, u1 - 3, r.bottom - thick - 2), e->accent);
    }
}

static void draw(HDC dc, int W, int H) {
    Theme th = app_theme(1); Layout L; do_layout(W, H, &L); g_last_layout = L;
    if (cfg_bool("glass")) g_glass_blit(dc, 0, W, H, th.bg); else g_fill(dc, R(0, 0, W, H), th.bg);
    SetBkMode(dc, TRANSPARENT); HGDIOBJ old_font = SelectObject(dc, g_font);

    if (g_hover == H_START || g_start_visible) hl(dc, L.start, &th, 0);
    draw_start_glyph(dc, (L.start.left + L.start.right) / 2, (L.start.top + L.start.bottom) / 2, (int)(H * 0.56), &th, g_hover == H_START);

    int isz = px(cfg_int("icon_size")), dragged = -1;
    for (int i = 0; i < g_nent; i++) {
        if (g_drag && !wcscmp(g_entries[i].key, g_drag_key)) { dragged = i; continue; }
        draw_entry(dc, &g_entries[i], L.ent[i], isz, &th, H, 0);
    }
    if (dragged >= 0) {
        RECT r = L.ent[dragged]; int w = rw(&r), x = g_drag_x - g_drag_dx;
        draw_entry(dc, &g_entries[dragged], R(x, r.top, x + w, r.bottom), isz, &th, H, 1);
    }
    if (cfg_bool("show_separator")) {
        const char *tk = cfg_str("divider_thickness");
        int tw = px(!strcmp(tk, "thin") ? 1 : !strcmp(tk, "mid") ? 2 : 3);
        for (int i = 1; i < g_nent; i++)
            if (g_entries[i - 1].pinned && !g_entries[i].pinned) {
                int sx = L.ent[i].left;
                g_alpha(dc, R(sx - tw / 2, (int)(H * 0.2), sx - tw / 2 + tw, (int)(H * 0.8)), th.text, 130);
                break;
            }
    }
    format_clock(g_clock_text, 64);
    RECT calc = L.clock; DrawTextW(dc, g_clock_text, -1, &calc, DT_CALCRECT | DT_CENTER | DT_NOPREFIX);
    int th_ = calc.bottom - calc.top, top = L.clock.top + ((L.clock.bottom - L.clock.top - th_) / 2 > 0 ? (L.clock.bottom - L.clock.top - th_) / 2 : 0);
    g_text(dc, g_clock_text, R(L.clock.left, top, L.clock.right, top + th_), DT_CENTER | DT_NOPREFIX, th.text);

    if (cfg_bool("show_preview_button")) {
        if (g_hover == H_CLOSEALL) hl(dc, L.closeall, &th, 0);
        int cx = (L.closeall.left + L.closeall.right) / 2, cy = (L.closeall.top + L.closeall.bottom) / 2, s = px(6);
        g_frame(dc, R(cx - s + 3, cy - s - 2, cx + s + 4, cy + s - 3), th.text, 1);
        g_fill(dc, R(cx - s + 3, cy - s - 2, cx + s + 4, cy - s), th.text);
        g_fill(dc, R(cx - s - 2, cy - s + 3, cx + s - 1, cy + s + 3), th.accent);
        g_frame(dc, R(cx - s - 2, cy - s + 3, cx + s - 1, cy + s + 3), th.text, 1);
    }
    if (L.kbd.right > L.kbd.left) {
        if (g_hover == H_KBD) hl(dc, L.kbd, &th, 0);
        int kx = (L.kbd.left + L.kbd.right) / 2, ky = (L.kbd.top + L.kbd.bottom) / 2, kwid = px(9), kh = px(5);
        g_frame(dc, R(kx - kwid, ky - kh, kx + kwid + 1, ky + kh + 2), th.text, 1);
        int rows[2] = { ky - kh + 2, ky };
        for (int r = 0; r < 2; r++) for (int i = 0; i < 5; i++)
            g_fill(dc, R(kx - kwid + 2 + i * px(3), rows[r], kx - kwid + 3 + i * px(3), rows[r] + 1), th.text);
        g_fill(dc, R(kx - px(4), ky + kh - 1, kx + px(5), ky + kh), th.text);
    }
    if (L.tray.right > L.tray.left) tray_draw(dc, L.tray);
    if (g_hover == H_DESK) hl(dc, L.desk, &th, 0);
    g_fill(dc, R(L.desk.left, 4, L.desk.left + 1, H - 4), th.active);
    SelectObject(dc, old_font);
}

static void paint(void) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(g_bar_hwnd, &ps); int W, H; client(&W, &H);
    if (W > 0 && H > 0) {
        HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HGDIOBJ old = SelectObject(mem, bmp); draw(mem, W, H);
        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
    }
    EndPaint(g_bar_hwnd, &ps);
}

/* ------------------------------------------------------------ interaction */
static int popup(const wchar_t **lab, const int *id, const int *fl, int n) {
    RECT b = bar_rect(); UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON; int y = b.top;
    if (!strcmp(cfg_str("position"), "bottom")) flags |= TPM_BOTTOMALIGN; else y = b.bottom;
    return app_popup(g_bar_hwnd, lab, id, fl, n, flags, POPUP_CUR, y);
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
            wchar_t t[256]; GetWindowTextW(g_entries[i].tasks[j], t, 256);
            if (!t[0]) lstrcpyW(t, L"(window)");
            lstrcpynW(titles[n], t, 60);
            hw[n] = g_entries[i].tasks[j]; lab[n] = titles[n]; id[n] = 3000 + n; fl[n] = 0; n++;
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
        case 5: if (h) PostMessageW(h, WM_CLOSE, 0, 0); break;
        case 6: for (int i = 0; i < nt; i++) PostMessageW(tasks[i], WM_CLOSE, 0, 0); break;
        case 7: if (h) SetWindowPos(h, topmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); break;
        case 8: if (pinned) bar_unpin(key); else bar_pin(path); break;
        case 9: { wchar_t ex[MAX_PATH], prm[MAX_PATH + 16]; exe_for(path, ex, MAX_PATH);
                  _snwprintf(prm, MAX_PATH + 16, L"/select,\"%ls\"", ex); app_run(L"explorer.exe", prm, SW_SHOWNORMAL); break; }
        case 10: if (h) { DWORD pid = 0; wchar_t prm[64]; GetWindowThreadProcessId(h, &pid);
                          _snwprintf(prm, 64, L"/F /PID %lu", (unsigned long)pid); app_run(L"taskkill.exe", prm, SW_HIDE); } break;
    }
}

void bar_show_desktop(void) {
    if (g_nhidden) {
        for (int i = 0; i < g_nhidden; i++) ShowWindow(g_hidden[i], SW_RESTORE);
        g_nhidden = 0;
    } else {
        for (int i = 0; i < g_nent; i++) for (int j = 0; j < g_entries[i].ntasks; j++) {
            HWND h = g_entries[i].tasks[j];
            if (!IsIconic(h) && g_nhidden < MAX_WINS) { ShowWindow(h, SW_SHOWMINNOACTIVE); g_hidden[g_nhidden++] = h; }
        }
    }
}

void bar_close_all_windows(void) {
    int count = 0; for (int i = 0; i < g_nent; i++) count += g_entries[i].ntasks;
    if (!count) return;
    wchar_t msg[96]; _snwprintf(msg, 96, L"Close all %d open window%ls?", count, count == 1 ? L"" : L"s");
    if (app_confirm(g_bar_hwnd, L"Close all windows", msg))
        for (int i = 0; i < g_nent; i++) for (int j = 0; j < g_entries[i].ntasks; j++) PostMessageW(g_entries[i].tasks[j], WM_CLOSE, 0, 0);
}

static void bar_menu(void) {
    const wchar_t *lab[] = { L"Show desktop", L"Close all windows", L"Edit config file", L"Reload settings", NULL, L"Exit taskbar" };
    int id[] = { 1, 6, 3, 4, -1, 5 }, fl[] = { 0, 0, 0, 0, 0, 0 };
    int cmd = popup(lab, id, fl, 6);
    if (cmd == 1) bar_show_desktop();
    else if (cmd == 6) bar_close_all_windows();
    else if (cmd == 3) { wchar_t prm[MAX_PATH + 4]; _snwprintf(prm, MAX_PATH + 4, L"\"%ls\"", g_config_path); app_run(L"notepad.exe", prm, SW_SHOWNORMAL); }
    else if (cmd == 4) app_reload();
    else if (cmd == 5) PostMessageW(g_bar_hwnd, WM_CLOSE, 0, 0);
}

static void reorder_drag(int x) {
    int W, H; client(&W, &H); Layout L; do_layout(W, H, &L);
    int di = entry_index(g_drag_key); if (di < 0) return;
    int bw = rw(&L.ent[di]), center = x - g_drag_dx + bw / 2, pos = 0;
    for (int i = 0; i < g_nent; i++) if (i != di && (L.ent[i].left + L.ent[i].right) / 2 < center) pos++;
    if (pos == di) return;
    Entry tmp = g_entries[di];
    if (pos < di) memmove(&g_entries[pos + 1], &g_entries[pos], sizeof(Entry) * (size_t)(di - pos));
    else memmove(&g_entries[di], &g_entries[di + 1], sizeof(Entry) * (size_t)(pos - di));
    g_entries[pos] = tmp;
}

static void finish_drag(void) {
    g_drag = 0;
    const char *paths[MAX_ENTRIES]; char *own[MAX_ENTRIES]; int n = 0;
    for (int i = 0; i < g_nent; i++) if (g_entries[i].pinned) { own[n] = utf8_from_w(g_entries[i].path); paths[n] = own[n]; n++; }
    cfg_list_set("pinned", paths, n);
    for (int i = 0; i < n; i++) free(own[i]);
    app_save(); bar_invalidate();
}

/* --------------------------------------------- fullscreen / reveal logic */
static void force_front(int hard) {
    if (!g_bar_hwnd || g_bar_hidden) return;
    UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    if (hard) SetWindowPos(g_bar_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, f);
    SetWindowPos(g_bar_hwnd, HWND_TOPMOST, 0, 0, 0, 0, f);
}

static void reveal(int seconds) {
    g_reveal_len = seconds ? seconds : cfg_int("reveal_seconds");
    g_reveal_until = GetTickCount() + (DWORD)g_reveal_len * 1000;
    if (g_bar_hidden) { g_bar_hidden = 0; ShowWindow(g_bar_hwnd, SW_SHOWNA); }
    force_front(1);
    const char *anim = cfg_str("reveal_animation");
    if (strcmp(anim, "none") && cfg_bool("animations")) { g_reveal_alpha = 0; SetTimer(g_bar_hwnd, 9, 30, NULL); }
    bar_invalidate();
}

/* returns 0 none, 1 maximised, 2 full-screen */
static int classify_fg(HWND *out) {
    HWND fg = GetForegroundWindow(); *out = fg;
    if (!fg || fg == g_bar_hwnd || !IsWindowVisible(fg) || IsIconic(fg)) return 0;
    wchar_t cls[128]; GetClassNameW(fg, cls, 128);
    if (is_ignored_class(cls) || !wcsncmp(cls, L"WinShell", 8)) return 0;
    RECT rc; GetWindowRect(fg, &rc); int sw, sh; screen_size(&sw, &sh);
    int covers = rc.left <= 0 && rc.top <= 0 && rc.right >= sw && rc.bottom >= sh;
    int captioned = (GetWindowLongPtrW(fg, GWL_STYLE) & WS_CAPTION) == WS_CAPTION;
    if (IsZoomed(fg) && captioned) return 1;
    if (covers) return 2;
    if (IsZoomed(fg)) return 1;
    return 0;
}

static int cursor_in_bar(int grow) {
    POINT pt; GetCursorPos(&pt); RECT b = bar_rect();
    return pt.x >= b.left - grow && pt.x <= b.right + grow && pt.y >= b.top - grow && pt.y <= b.bottom + grow;
}
static int edge_touch(void) {
    POINT pt; GetCursorPos(&pt); int sw, sh; screen_size(&sw, &sh);
    return !strcmp(cfg_str("position"), "bottom") ? pt.y >= sh - 1 : pt.y <= 0;
}
static int fg_excluded(HWND fg) {
    JNode *ex = cfg_node("strip_exclude"); if (!fg || !ex || ex->t != J_ARR || !ex->child) return 0;
    wchar_t pe[MAX_PATH], key[MAX_PATH]; window_process_path(fg, pe, MAX_PATH); if (!pe[0]) return 0;
    app_key(pe, key, MAX_PATH);
    for (JNode *c = ex->child; c; c = c->next) {
        wchar_t p[MAX_PATH], pe2[MAX_PATH], k2[MAX_PATH];
        w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe2, MAX_PATH); app_key(pe2, k2, MAX_PATH);
        if (!wcscmp(key, k2)) return 1;
    }
    return 0;
}
static int fg_overlaps_bar(void) {
    HWND fg = GetForegroundWindow(); RECT rc, b = bar_rect();
    if (!fg || fg == g_bar_hwnd || !GetWindowRect(fg, &rc)) return 0;
    wchar_t cls[128]; GetClassNameW(fg, cls, 128); if (!wcsncmp(cls, L"WinShell", 8)) return 0;
    return rc.left < b.right && rc.right > b.left && rc.top < b.bottom && rc.bottom > b.top;
}

static void check_fullscreen(void) {
    HWND fg; int state = classify_fg(&fg); DWORD now = GetTickCount();
    const char *mode = state == 2 ? cfg_str("fs_mode") : (state == 1 ? cfg_str("max_mode") : NULL);
    g_cur_hide_mode = mode && (!strcmp(mode, "hide") || !strcmp(mode, "float"));
    int exc = state ? fg_excluded(fg) : 0;
    int is_show = mode && (!strcmp(mode, "show") || !strcmp(mode, "resize"));
    int auto_hide = cfg_bool("bar_autohide") && !exc && !is_show;
    int hide = (mode && (!strcmp(mode, "hide") || !strcmp(mode, "float"))) || auto_hide;
    if (hide && (int)(g_reveal_until - now) > 0) {
        hide = 0;
        if (g_menu_depth > 0 || g_start_visible || cursor_in_bar(6)) g_reveal_until = now + (DWORD)g_reveal_len * 1000;
    }
    g_fs_force = state && is_show;
    if (mode && !strcmp(mode, "resize") && state == 2) {
        RECT w = work_rect();
        SetWindowPos(fg, NULL, w.left, w.top, w.right - w.left, w.bottom - w.top, SWP_NOACTIVATE | SWP_NOZORDER);
    }
    if (hide != g_bar_hidden) {
        g_bar_hidden = hide; ShowWindow(g_bar_hwnd, hide ? SW_HIDE : SW_SHOWNA);
        if (!hide) { force_front(1); bar_invalidate(); }
    }
}

static void fast_tick(void) {
    g_fast++;
    if (!g_bar_hwnd) return;
    if (g_fast % 4 == 0) { if (tray_refresh()) bar_invalidate(); }
    if (!g_bar_hidden && g_menu_depth == 0) {
        int hard = g_need_hard; g_need_hard = 0;
        if (!hard) {
            if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && g_fast % 3 == 0 && cursor_in_bar(60)) hard = 1;
            else if (g_fs_force && g_fast % 12 == 0) hard = 1;
            else if (g_fast % 3 == 0 && fg_overlaps_bar()) hard = 1;
        }
        force_front(hard);
    } else if (g_bar_hidden) {
        if (g_cur_hide_mode && g_fast % 4 == 0 && edge_touch()) reveal(0);
    }
}

static void on_timer(void) {
    g_tick++; int dirty = 0;
    if (cfg_mtime() != app_config_mtime()) app_reload();
    if (g_tick % 2 == 0) bar_refresh(0);
    check_fullscreen();
    HWND fg = GetForegroundWindow();
    if (fg != g_last_fg) { g_last_fg = fg; dirty = 1; g_need_hard = 1; }
    wchar_t now[64]; format_clock(now, 64); if (wcscmp(now, g_clock_text)) dirty = 1;
    if (dirty) bar_invalidate();
}

/* ------------------------------------------------------------ window proc */
static void on_move(int x, int kind, int idx) {
    if (g_press && !g_drag && abs(x - g_press_x) > px(6)) {
        int W, H; client(&W, &H); Layout L; do_layout(W, H, &L);
        int i = entry_index(g_press_key);
        if (i >= 0) { g_drag_dx = x - L.ent[i].left; g_drag = 1; lstrcpyW(g_drag_key, g_press_key); }
    }
    if (g_drag) { g_drag_x = x; reorder_drag(x); bar_invalidate(); return; }
    int nk = kind == H_ENTRY ? H_ENTRY : (kind == H_START || kind == H_DESK || kind == H_CLOSEALL || kind == H_KBD ? kind : H_NONE);
    wchar_t nkey[MAX_PATH]; nkey[0] = 0; if (nk == H_ENTRY && idx >= 0) lstrcpyW(nkey, g_entries[idx].key);
    if (!g_tracking) { TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, g_bar_hwnd, 0 }; TrackMouseEvent(&t); g_tracking = 1; }
    if (nk != g_hover || wcscmp(nkey, g_hover_key)) { g_hover = nk; lstrcpyW(g_hover_key, nkey); bar_invalidate(); }
}

static void on_left_up(int kind, int idx) {
    if (g_press) {
        ReleaseCapture(); wchar_t pk[MAX_PATH]; lstrcpyW(pk, g_press_key); g_press = 0;
        if (g_drag) { finish_drag(); return; }
        if (kind == H_ENTRY && idx >= 0 && !wcscmp(g_entries[idx].key, pk)) click_entry(&g_entries[idx]);
        return;
    }
    if (kind == H_START) start_toggle();
    else if (kind == H_DESK) bar_show_desktop();
    else if (kind == H_CLOSEALL) window_list_menu(NULL);
    else if (kind == H_KBD) app_run(L"osk.exe", NULL, SW_SHOWNORMAL);
}

static LRESULT CALLBACK bar_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_shellhook_msg && msg == g_shellhook_msg) {
        UINT event = (UINT)(wp & 0x7FFF);
        if (wp == 0x8006) flash_add((HWND)lp); else if (event == 4) flash_del((HWND)lp);
        g_need_hard = 1; bar_refresh(event == 6); bar_invalidate(); return 0;
    }
    switch (msg) {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_TIMER:
        if (wp == 2) fast_tick();
        else if (wp == 9) {
            g_reveal_alpha += 40; if (g_reveal_alpha > 255) g_reveal_alpha = 255;
            apply_glass(hwnd, cfg_bool("glass"), cfg_int("glass_opacity") * g_reveal_alpha / 255);
            if (g_reveal_alpha >= 255) KillTimer(hwnd, 9);
        } else on_timer();
        return 0;
    case WM_COPYDATA: {
        COPYDATASTRUCT *cds = (COPYDATASTRUCT *)lp;
        if (cds && cds->dwData == 1 && cds->lpData) app_run(L"explorer.exe", (const wchar_t *)cds->lpData, SW_SHOWNORMAL);
        return 1;
    }
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp), idx; int kind = hit(x, y, &idx);
        if (msg == WM_MOUSEMOVE) on_move(x, kind, idx);
        else if (msg == WM_LBUTTONDOWN) { if (kind == H_ENTRY && idx >= 0) { g_press = 1; g_press_x = x; lstrcpyW(g_press_key, g_entries[idx].key); SetCapture(hwnd); } }
        else if (msg == WM_LBUTTONUP) { if (kind == H_TRAY) tray_click(x, y, 0); else on_left_up(kind, idx); }
        else if (msg == WM_LBUTTONDBLCLK) { if (kind == H_TRAY) tray_click(x, y, 2); }
        else if (msg == WM_RBUTTONUP) {
            if (kind == H_TRAY) tray_click(x, y, 1);
            else if (kind == H_ENTRY && idx >= 0) entry_menu(&g_entries[idx]);
            else if (kind == H_BAR || kind == H_DESK || kind == H_START || kind == H_CLOSEALL) bar_menu();
        } else if (msg == WM_MBUTTONUP && kind == H_ENTRY && idx >= 0 && g_entries[idx].path[0]) app_run(g_entries[idx].path, NULL, SW_SHOWNORMAL);
        return 0;
    }
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
    RECT b = bar_rect(); int w = rw(&b), h = b.bottom - b.top, x = b.left, y = b.top;
    int detach = cfg_int("bar_detach");
    if (detach && !strcmp(cfg_str("position"), "bottom")) y -= px(detach);
    else if (detach && !strcmp(cfg_str("position"), "top")) y += px(detach);
    SetWindowPos(g_bar_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    if (cfg_bool("bar_corner_round") && cfg_bool("corner_round")) {
        int r = px(16); SetWindowRgn(g_bar_hwnd, CreateRoundRectRgn(0, 0, w + 1, h + 1, r, r), TRUE);
    } else SetWindowRgn(g_bar_hwnd, NULL, TRUE);
    apply_work_area();
    apply_glass(g_bar_hwnd, cfg_bool("glass"), cfg_int("glass_opacity"));
    HWND wine = FindWindowW(L"Shell_TrayWnd", NULL);
    if (wine) {
        if (cfg_bool("hide_wine_taskbar") && cfg_bool("tray_icons")) {
            SetWindowPos(wine, NULL, -32000, -32000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            ShowWindow(wine, SW_SHOWNOACTIVATE);
        } else ShowWindow(wine, cfg_bool("hide_wine_taskbar") ? SW_HIDE : SW_SHOW);
    }
    bar_rebuild_pins(); bar_refresh(1); bar_invalidate();
}

int bar_create(void) {
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.style = CS_DBLCLKS; wc.lpfnWndProc = bar_proc; wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);
    g_shellhook_msg = RegisterWindowMessageW(L"SHELLHOOK");
    RECT b = bar_rect();
    g_bar_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, CLASS_NAME, L"WinShell Taskbar",
                                 WS_POPUP | WS_VISIBLE, b.left, b.top, rw(&b), b.bottom - b.top, NULL, NULL, wc.hInstance, NULL);
    if (!g_bar_hwnd) return 0;
    SetTimer(g_bar_hwnd, 1, 1000, NULL); SetTimer(g_bar_hwnd, 2, 40, NULL);
    ShellHook_t reg = (ShellHook_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "RegisterShellHookWindow");
    if (!reg || !reg(g_bar_hwnd)) { /* timer-only mode */ }
    bar_apply();
    return 1;
}
