/* WinShell Start panel: Windows 10 tiles, Windows 7 / Zorin two-column with links pane,
 * Windows 11 / macOS / GNOME icon grid, Windows 98 classic. */
#include "app.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#define CLASS_NAME L"WinShellStart"
#define EDIT_ID 200
#define MAX_APPS 1400
#define MAX_ROWS 1700
#define MAX_PINS 64
#define MAX_EXP 64

enum { LY_TILES, LY_TWOCOL, LY_GRID, LY_CLASSIC };
enum { HK_NONE, HK_RAIL, HK_APP, HK_RUN, HK_GROUP, HK_TILE, HK_LINK, HK_USER, HK_ADD, HK_SW, HK_ALL, HK_SEARCH };

typedef struct { wchar_t name[128]; wchar_t path[MAX_PATH]; char group[96]; } App;
typedef struct { int kind, level; wchar_t path[MAX_PATH]; wchar_t name[128]; char group[96]; } Row;   /* kind 0 head 1 app 2 run 3 group */
typedef struct { wchar_t path[MAX_PATH]; RECT r; int sw, sh; } Tile;
typedef struct { const char *id, *label; const wchar_t *target, *params; int glyph; } PaneLink;

static HWND g_hwnd, g_edit; static WNDPROC g_edit_old;
int g_start_visible;
static DWORD g_closed_at;
static wchar_t g_query[128];
static App g_apps[MAX_APPS]; static int g_napps;
static Row g_rows[MAX_ROWS]; static int g_nrows;
static Tile g_tiles[MAX_PINS]; static int g_ntiles;
static int g_scroll, g_tile_scroll, g_tile_content, g_grid_all, g_shuffle_seed;
static int g_hover_kind; static wchar_t g_hover_path[MAX_PATH];
static char g_exp[MAX_EXP][96]; static int g_nexp;
static int g_ed_el = -1, g_ed_sx, g_ed_sy, g_ed_dx, g_ed_dy;
static const int RAIL_EL[6] = { E_SM_COLLAPSE, E_SM_PROGRAMS, E_SM_SYSTEM, E_SM_RUN, E_SM_SETTINGS, E_SM_POWER };
static const int RAIL_GLYPH[6] = { G_COLLAPSE, G_PROGRAMS, G_SYSTEM, G_RUN, G_GEAR, G_POWER };

static const struct { const wchar_t *n, *e; } BUILTIN[] = {
    { L"Notepad", L"notepad.exe" }, { L"Command Prompt", L"cmd.exe" }, { L"Registry Editor", L"regedit.exe" }, { L"Task Manager", L"taskmgr.exe" },
    { L"Control Panel", L"control.exe" }, { L"File Explorer", L"explorer.exe" }, { L"Wine Configuration", L"winecfg.exe" }, { L"WordPad", L"wordpad.exe" } };
static const COLORREF TILE_COLORS[] = { RGB(0,120,215), RGB(16,137,62), RGB(218,59,1), RGB(142,140,216), RGB(0,153,188), RGB(194,57,179), RGB(209,52,56),
    RGB(255,140,0), RGB(116,77,169), RGB(0,178,148), RGB(231,72,86), RGB(104,118,138), RGB(73,130,5), RGB(177,70,194), RGB(0,99,177) };
static const int TILE_PX[8] = { 60, 72, 84, 96, 110, 124, 140, 160 };
static const PaneLink LINKS[] = {
    { "documents", "Documents", L"%USERPROFILE%\\Documents", NULL, G_DOC }, { "pictures", "Pictures", L"%USERPROFILE%\\Pictures", NULL, G_PIC },
    { "music", "Music", L"%USERPROFILE%\\Music", NULL, G_MUSIC }, { "videos", "Videos", L"%USERPROFILE%\\Videos", NULL, G_FOLDER },
    { "downloads", "Downloads", L"%USERPROFILE%\\Downloads", NULL, G_FOLDER }, { "computer", "Computer", L"explorer.exe", L"/e,::{20D04FE0-3AEA-1069-A2D8-08002B30309D}", G_PC },
    { "controlpanel", "Control Panel", L"control.exe", NULL, G_PANEL }, { "devices", "Devices and Printers", L"control.exe", L"printers", G_PC },
    { "taskmgr", "Task Manager", L"taskmgr.exe", NULL, G_SYSTEM }, { "run", "Run...", L"", NULL, G_RUN }, { "help", "Help and Support", L"winhlp32.exe", NULL, G_HELP },
    { "settings", "WinShell Settings", L"", NULL, G_GEAR } };
#define NLINKS ((int)(sizeof LINKS / sizeof LINKS[0]))
int pane_link_count(void) { return NLINKS; }
const char *pane_link_id(int i) { return LINKS[i].id; }
const char *pane_link_label(int i) { return LINKS[i].label; }
int pane_link_glyph(int i) { return LINKS[i].glyph; }

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int collapsed(void) { return cfg_bool("start_list_collapsed"); }
static int is_full(void) { return !strcmp(cfg_str("start_size"), "full"); }
static int gap(void) { return is_full() ? 0 : px(8); }
HWND start_hwnd(void) { return g_hwnd; }
const wchar_t *start_query(void) { return g_query; }

static int layout_mode(void) {
    const char *t = cfg_str("start_theme");
    if (!strcmp(t, "windows11") || !strcmp(t, "macos") || !strcmp(t, "gnome") || !strcmp(t, "ubuntu")) return LY_GRID;
    if (!strcmp(t, "windows7") || !strcmp(t, "windowsxp") || !strcmp(t, "zorin") || !strcmp(t, "mint") || !strcmp(t, "kde") || !strcmp(t, "xfce") || !strcmp(t, "arch")) return LY_TWOCOL;
    if (!strcmp(t, "windows98")) return LY_CLASSIC;
    return LY_TILES;
}
static double size_factor(void) {
    const char *s = cfg_str("start_size");
    if (!strcmp(s, "tiny")) return 0.8; if (!strcmp(s, "small")) return 0.9; if (!strcmp(s, "large")) return 1.15; if (!strcmp(s, "huge")) return 1.3; return 1.0;
}
static int pane_on(void) { return !cfg_bool("start_tiles_collapsed") && el_show(E_SM_TILES) && layout_mode() != LY_CLASSIC; }
static int list_on(void) { return !collapsed() && layout_mode() != LY_GRID; }
static int cell_w(void) { int l = cfg_int("tile_level"); if (l < 1) l = 1; if (l > 8) l = 8; return px(TILE_PX[l - 1]) * cfg_int("tile_w_adj") / 100; }
static int cell_h(void) { int l = cfg_int("tile_level"); if (l < 1) l = 1; if (l > 8) l = 8; return px(TILE_PX[l - 1]) * cfg_int("tile_h_adj") / 100; }

/* ---------------------------------------------------------------- geometry */
typedef struct {
    int mode, W, H, list_top, vrail;
    RECT rail, list, pane, user, add, search, sw, all, ri[6], title;
    int has_list, has_pane, has_search;
} Geo;

static void offs(int e, RECT *r) { int dx, dy; el_offset(e, &dx, &dy); OffsetRect(r, px(dx), px(dy)); }

static void geometry(Geo *g) {
    memset(g, 0, sizeof *g); g->mode = layout_mode(); double f = size_factor(); int sw, sh; screen_size(&sw, &sh); RECT wk = work_rect();
    int rail = px(cfg_int("rail_w")), listw = px(cfg_int("list_w")), tilesw = (int)(px(330) * f), rowH = px(cfg_int("search_h"));
    const char *rp = cfg_str("start_rail_pos"); int hz = !strcmp(rp, "top") || !strcmp(rp, "bottom"); g->vrail = !hz;
    int full = is_full(), H0 = (int)(px(620) * f);
    if (g->mode == LY_GRID) {
        g->W = full ? sw : (int)(px(600) * f); g->H = full ? wk.bottom - wk.top : (int)(px(640) * f);
    } else if (g->mode == LY_TWOCOL) {
        g->W = full ? sw : px(cfg_int("list_w")) + px(cfg_int("pane_w")) + (int)(px(0)); g->H = full ? wk.bottom - wk.top : (int)(px(560) * f);
        if (!pane_on()) g->W = listw;
    } else {
        int rw_ = g->vrail ? rail : 0; g->W = rw_ + (list_on() ? listw : 0) + (pane_on() ? tilesw : 0);
        if (g->W < rw_ + px(120)) g->W = rw_ + px(260);
        g->H = H0; if (full) { g->W = sw; g->H = wk.bottom - wk.top; if (g->vrail) tilesw = g->W - rail - (list_on() ? listw : 0); else tilesw = g->W - (list_on() ? listw : 0); }
        if (hz) g->H += rail;
    }
    if (!full) { if (g->W > sw - gap() * 2) g->W = sw - gap() * 2; if (g->H > wk.bottom - wk.top - gap() * 2) g->H = wk.bottom - wk.top - gap() * 2; }
    int W = g->W, H = g->H; (void)tilesw;

    if (g->mode == LY_TILES || g->mode == LY_CLASSIC) {
        RECT body = R(0, 0, W, H);
        if (!strcmp(rp, "left")) { g->rail = R(0, 0, rail, H); body.left = rail; } else if (!strcmp(rp, "right")) { g->rail = R(W - rail, 0, W, H); body.right = W - rail; }
        else if (!strcmp(rp, "top")) { g->rail = R(0, 0, W, rail); body.top = rail; } else { g->rail = R(0, H - rail, W, H); body.bottom = H - rail; }
        int left_pane = !strcmp(cfg_str("start_tiles_side"), "left");
        g->has_list = list_on(); g->has_pane = pane_on();
        int lw = g->has_list ? (g->has_pane ? listw : body.right - body.left) : 0;
        if (g->has_list && g->has_pane) { if (left_pane) { g->list = R(body.right - lw, body.top, body.right, body.bottom); g->pane = R(body.left, body.top, body.right - lw, body.bottom); }
            else { g->list = R(body.left, body.top, body.left + lw, body.bottom); g->pane = R(body.left + lw, body.top, body.right, body.bottom); } }
        else if (g->has_list) g->list = body; else if (g->has_pane) g->pane = body;
        if (g->has_list) {
            g->has_search = 1;
            g->user = R(g->list.left + px(8), g->list.top + px(8), g->list.right - px(52), g->list.top + px(56));
            g->add = R(g->list.right - px(42), g->list.top + px(12), g->list.right - px(10), g->list.top + px(44));
            int sh_ = px(cfg_int("search_h")); g->search = R(g->list.left + px(10), g->list.top + px(60), g->list.right - px(10), g->list.top + px(60) + sh_);
            g->list_top = g->search.bottom - g->list.top + px(6);
        }
        if (g->has_pane) { g->title = R(g->pane.left + px(12), g->pane.top + px(6), g->pane.right - px(8), g->pane.top + px(38)); g->sw = R(g->pane.right - px(40), g->pane.top + px(6), g->pane.right - px(8), g->pane.top + px(38)); }
        /* rail items: packed from the start, power at the far end */
        int n = 0, cell = rail; int order[6] = { 0, 1, 2, 3, 4, 5 };
        for (int k = 0; k < 6; k++) { int i = order[k]; int visible = el_show(RAIL_EL[i]); int pos;
            if (!visible) continue;
            if (i == 5) pos = (g->vrail ? H : W) - cell - px(2); else { pos = n * cell; n++; }
            g->ri[i] = g->vrail ? R(g->rail.left, pos, g->rail.right, pos + cell) : R(pos, g->rail.top, pos + cell, g->rail.bottom); }
    } else if (g->mode == LY_TWOCOL) {
        g->has_list = list_on(); g->has_pane = pane_on(); int lw = g->has_pane ? listw : W; int foot = px(48);
        if (g->has_list) { g->list = R(0, 0, lw, H); g->has_search = 1; g->search = R(px(10), H - px(8) - rowH, lw - px(10), H - px(8)); g->list_top = px(8); g->list.bottom = g->search.top - px(8); }
        if (g->has_pane) { g->pane = R(lw, 0, W, H); g->user = R(g->pane.left + px(10), g->pane.top + px(10), g->pane.right - px(10), g->pane.top + px(10) + cfg_int("avatar_size") * 3 / 2);
            g->title = R(g->pane.left + px(12), g->user.bottom + px(4), g->pane.right - px(40), g->user.bottom + px(34)); g->sw = R(g->pane.right - px(40), g->title.top, g->pane.right - px(8), g->title.bottom);
            int cell = px(36), x = g->pane.right - px(8) - cell;
            for (int i = 5; i >= 0; i--) { if (!el_show(RAIL_EL[i])) continue; g->ri[i] = R(x, H - foot + px(6), x + cell, H - foot + px(6) + cell); x -= cell; } }
        g->add = R(0, 0, 0, 0);
    } else {                                                              /* grid */
        g->has_list = 0; g->has_pane = 1; g->has_search = 1; int foot = px(64);
        g->search = R(px(24), px(18), W - px(24), px(18) + rowH + px(6));
        g->title = R(px(28), g->search.bottom + px(10), W - px(160), g->search.bottom + px(44));
        g->sw = R(W - px(44), g->title.top, W - px(12), g->title.bottom); g->all = R(W - px(150), g->title.top, W - px(50), g->title.bottom);
        g->pane = R(0, g->title.bottom, W, H - foot); g->list = g->pane; g->list_top = 0;
        g->user = R(px(24), H - foot + px(10), px(24) + px(220), H - px(10));
        int cell = px(40), x = W - px(16) - cell;
        for (int i = 5; i >= 0; i--) { if (!el_show(RAIL_EL[i])) continue; if (i == 0 || i == 3) continue; g->ri[i] = R(x, H - foot + px(12), x + cell, H - foot + px(12) + cell); x -= cell; }
        g->add = R(0, 0, 0, 0);
    }
    if (!el_show(E_SM_USER)) g->user = R(0, 0, 0, 0); if (!el_show(E_SM_ADD)) g->add = R(0, 0, 0, 0);
    if (!el_show(E_SM_SEARCH) || cfg_bool("start_search_in_bar")) g->has_search = 0;
    if (!el_show(E_SM_SWITCH)) g->sw = R(0, 0, 0, 0);
    if (g->user.right > g->user.left) offs(E_SM_USER, &g->user); if (g->add.right > g->add.left) offs(E_SM_ADD, &g->add);
    if (g->has_search || g->search.right > g->search.left) offs(E_SM_SEARCH, &g->search);
    if (g->sw.right > g->sw.left) offs(E_SM_SWITCH, &g->sw);
    for (int i = 0; i < 6; i++) if (g->ri[i].right > g->ri[i].left) offs(RAIL_EL[i], &g->ri[i]);
}
static void position(int *x, int *y, int *W, int *H) {
    Geo g; geometry(&g); *W = g.W; *H = g.H; int sw, sh; screen_size(&sw, &sh); RECT wk = work_rect(), sb = bar_start_screen_rect(); int gp = gap();
    if (is_full()) { *x = wk.left; *y = wk.top; return; }
    if (bar_vertical()) {
        *x = !strcmp(cfg_str("position"), "left") ? wk.left + gp : wk.right - g.W - gp;
        int cy = (sb.top + sb.bottom) / 2; *y = cy < sh / 2 ? sb.top : sb.bottom - g.H;
        if (*y > wk.bottom - g.H - gp) *y = wk.bottom - g.H - gp; if (*y < wk.top + gp) *y = wk.top + gp;
    } else {
        int cx = (sb.left + sb.right) / 2; if (cx < sw / 3) *x = sb.left < gp ? gp : sb.left; else if (cx > sw * 2 / 3) *x = sb.right - g.W; else *x = sb.left;
        if (*x > sw - g.W - gp) *x = sw - g.W - gp; if (*x < gp) *x = gp;
        *y = !strcmp(cfg_str("position"), "bottom") ? wk.bottom - g.H - gp : wk.top + gp;
    }
}

/* ---------------------------------------------------------------- app list */
static int cmp_app(const void *a, const void *b) { return _wcsicmp(((const App *)a)->name, ((const App *)b)->name); }
static int app_known(const wchar_t *name) { for (int i = 0; i < g_napps; i++) if (!_wcsicmp(g_apps[i].name, name)) return i; return -1; }
static void app_set(const wchar_t *name, const wchar_t *path, const char *group, int override) {
    int i = app_known(name);
    if (i >= 0) { if (override) lstrcpynW(g_apps[i].path, path, MAX_PATH); return; }
    if (g_napps >= MAX_APPS) return;
    lstrcpynW(g_apps[g_napps].name, name, 128); lstrcpynW(g_apps[g_napps].path, path, MAX_PATH); lstrcpynA(g_apps[g_napps].group, group, 96); g_napps++;
}
static void walk(const wchar_t *dir, int depth, const char *group) {
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; _snwprintf(pat, MAX_PATH, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t full[MAX_PATH]; _snwprintf(full, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 4) { char g2[96]; if (depth == 0) { char *u = utf8_from_w(fd.cFileName); lstrcpynA(g2, u, 96); free(u); } else lstrcpynA(g2, group, 96); walk(full, depth + 1, g2); }
        } else { size_t l = wcslen(fd.cFileName);
            if (l > 4 && !_wcsicmp(fd.cFileName + l - 4, L".lnk")) { wchar_t name[128]; lstrcpynW(name, fd.cFileName, 128); name[wcslen(name) - 4] = 0; if (_wcsnicmp(name, L"uninstall", 9) != 0) app_set(name, full, group, 0); } }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void scan_program_files(const wchar_t *root, const char *label) {
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; _snwprintf(pat, MAX_PATH, L"%ls\\*", root); HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
        wchar_t sub[MAX_PATH], pat2[MAX_PATH]; _snwprintf(sub, MAX_PATH, L"%ls\\%ls", root, fd.cFileName); _snwprintf(pat2, MAX_PATH, L"%ls\\*.exe", sub);
        WIN32_FIND_DATAW f2; HANDLE h2 = FindFirstFileW(pat2, &f2); char *fu = utf8_from_w(fd.cFileName); char grp[96]; snprintf(grp, 96, "%s/%s", label, fu); free(fu);
        if (h2 != INVALID_HANDLE_VALUE) { do { wchar_t low[260]; lstrcpynW(low, f2.cFileName, 260); _wcslwr(low);
                if (wcsstr(low, L"unins") || wcsstr(low, L"setup") || wcsstr(low, L"crash") || wcsstr(low, L"update") || wcsstr(low, L"helper")) continue;
                wchar_t full[MAX_PATH], nm[128]; _snwprintf(full, MAX_PATH, L"%ls\\%ls", sub, f2.cFileName); lstrcpynW(nm, f2.cFileName, 128); nm[wcslen(nm) - 4] = 0;
                wchar_t key[160]; _snwprintf(key, 160, L"%ls (%ls)", nm, fd.cFileName); app_set(key, full, grp, 0); } while (FindNextFileW(h2, &f2)); FindClose(h2); }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void scan(void) {
    wchar_t root[MAX_PATH], base[MAX_PATH]; g_napps = 0;
    DWORD n = GetEnvironmentVariableW(L"PROGRAMDATA", base, MAX_PATH); if (!n || n >= MAX_PATH) lstrcpyW(base, L"C:\\ProgramData");
    _snwprintf(root, MAX_PATH, L"%ls\\Microsoft\\Windows\\Start Menu\\Programs", base); walk(root, 0, "");
    n = GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH);
    if (n && n < MAX_PATH) { _snwprintf(root, MAX_PATH, L"%ls\\Microsoft\\Windows\\Start Menu\\Programs", base); walk(root, 0, ""); }
    for (size_t i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++) { wchar_t full[MAX_PATH]; find_exe(BUILTIN[i].e, full, MAX_PATH); if (path_exists(full)) app_set(BUILTIN[i].n, full, "Windows tools", 0); }
    JNode *extra = cfg_node("start_apps");
    if (extra && extra->t == J_ARR) for (JNode *c = extra->child; c; c = c->next) { wchar_t p[MAX_PATH], nm[128]; w_from_utf8_buf(c->s, p, MAX_PATH); display_name(p, nm, 128); app_set(nm, p, "", 1); }
    if (cfg_bool("start_programs_mode")) {
        wchar_t sys[MAX_PATH]; DWORD k = GetEnvironmentVariableW(L"ProgramFiles", sys, MAX_PATH); scan_program_files(k && k < MAX_PATH ? sys : L"C:\\Program Files", "Program Files");
        k = GetEnvironmentVariableW(L"ProgramFiles(x86)", sys, MAX_PATH); if (k && k < MAX_PATH) scan_program_files(sys, "Program Files");
    }
    qsort(g_apps, (size_t)g_napps, sizeof(App), cmp_app);
}
static const wchar_t *name_of(const wchar_t *path, wchar_t *buf) { for (int i = 0; i < g_napps; i++) if (!_wcsicmp(g_apps[i].path, path)) return g_apps[i].name; display_name(path, buf, 128); return buf; }
static int path_known(const wchar_t *p) { for (int i = 0; i < g_napps; i++) if (!_wcsicmp(g_apps[i].path, p)) return 1; return 0; }
static void add_row(int kind, int level, const wchar_t *path, const wchar_t *name, const char *group) {
    if (g_nrows >= MAX_ROWS) return; Row *r = &g_rows[g_nrows++]; r->kind = kind; r->level = level;
    lstrcpynW(r->path, path, MAX_PATH); lstrcpynW(r->name, name, 128); lstrcpynA(r->group, group ? group : "", 96);
}
static int is_exp(const char *g) { for (int i = 0; i < g_nexp; i++) if (!strcmp(g_exp[i], g)) return 1; return 0; }
static void toggle_exp(const char *g) {
    for (int i = 0; i < g_nexp; i++) if (!strcmp(g_exp[i], g)) { memmove(g_exp[i], g_exp[i + 1], (size_t)(g_nexp - i - 1) * 96); g_nexp--; return; }
    if (g_nexp < MAX_EXP) lstrcpynA(g_exp[g_nexp++], g, 96);
}
typedef struct { wchar_t path[MAX_PATH]; int n; } Use;
static int cmp_use(const void *a, const void *b) { return ((const Use *)b)->n - ((const Use *)a)->n; }

static void build_programs_rows(void) {
    char tops[80][96]; int nt = 0;
    for (int i = 0; i < g_napps; i++) { char t[96]; lstrcpynA(t, g_apps[i].group, 96); char *s = strchr(t, '/'); if (s) *s = 0; if (!t[0]) continue;
        int f = 0; for (int k = 0; k < nt; k++) if (!strcmp(tops[k], t)) f = 1; if (!f && nt < 80) lstrcpynA(tops[nt++], t, 96); }
    for (int i = 0; i < g_napps; i++) if (!g_apps[i].group[0]) add_row(1, 0, g_apps[i].path, g_apps[i].name, "");
    for (int pass = 0; pass < 2; pass++) for (int k = 0; k < nt; k++) {
        int pf = !strcmp(tops[k], "Program Files"); if ((pass == 1) != pf) continue;
        wchar_t w[128]; w_from_utf8_buf(tops[k], w, 128); add_row(3, 0, L"", w, tops[k]); if (!is_exp(tops[k])) continue;
        char subs[120][96]; int ns = 0;
        for (int i = 0; i < g_napps; i++) { const char *g = g_apps[i].group; size_t tl = strlen(tops[k]); if (strncmp(g, tops[k], tl) != 0) continue;
            if (g[tl] == 0) add_row(1, 1, g_apps[i].path, g_apps[i].name, g);
            else if (g[tl] == '/') { int f = 0; for (int s = 0; s < ns; s++) if (!strcmp(subs[s], g)) f = 1; if (!f && ns < 120) lstrcpynA(subs[ns++], g, 96); } }
        for (int s = 0; s < ns; s++) { wchar_t sw_[128]; w_from_utf8_buf(strchr(subs[s], '/') + 1, sw_, 128); add_row(3, 1, L"", sw_, subs[s]);
            if (is_exp(subs[s])) for (int i = 0; i < g_napps; i++) if (!strcmp(g_apps[i].group, subs[s])) add_row(1, 2, g_apps[i].path, g_apps[i].name, subs[s]); }
    }
}

static void build_rows(void) {
    wchar_t q[128]; lstrcpynW(q, g_query, 128); _wcslwr(q); wchar_t *qs = q; while (*qs == L' ') qs++;
    for (int e = (int)wcslen(qs); e > 0 && qs[e - 1] == L' '; ) qs[--e] = 0;
    g_nrows = 0; g_scroll = 0; wchar_t nb[128];
    if (qs[0]) {
        for (int pass = 0; pass < 2; pass++) for (int i = 0; i < g_napps; i++) { wchar_t l[128]; lstrcpyW(l, g_apps[i].name); _wcslwr(l); if (!wcsstr(l, qs)) continue;
            int pre = wcsncmp(l, qs, wcslen(qs)) == 0; if ((pass == 0) == pre) add_row(1, 0, g_apps[i].path, g_apps[i].name, ""); }
        wchar_t label[160]; _snwprintf(label, 160, L"Run \"%ls\"", g_query); add_row(2, 0, g_query, label, ""); return;
    }
    if (cfg_bool("start_programs_mode")) { build_programs_rows(); return; }
    if (cfg_bool("start_show_recent")) { JNode *rec = cfg_node("app_recent"); int shown = 0;
        if (rec && rec->t == J_ARR) for (JNode *c = rec->child; c && shown < 3; c = c->next) { wchar_t p[MAX_PATH]; w_from_utf8_buf(c->s, p, MAX_PATH); if (!path_known(p)) continue;
            if (!shown) add_row(0, 0, L"", L"Recently used", ""); add_row(1, 0, p, name_of(p, nb), ""); shown++; } }
    if (cfg_bool("start_show_most_used")) { JNode *use = cfg_node("app_usage"); static Use u[128]; int n = 0;
        if (use && use->t == J_OBJ) for (JNode *c = use->child; c && n < 128; c = c->next) { wchar_t p[MAX_PATH]; w_from_utf8_buf(c->key, p, MAX_PATH); if (c->n > 0 && path_known(p)) { lstrcpyW(u[n].path, p); u[n].n = (int)c->n; n++; } }
        qsort(u, (size_t)n, sizeof(Use), cmp_use); if (n > 4) n = 4; if (n) add_row(0, 0, L"", L"Most used", "");
        for (int i = 0; i < n; i++) add_row(1, 0, u[i].path, name_of(u[i].path, nb), ""); }
    wchar_t last = 0;
    for (int i = 0; i < g_napps; i++) { wchar_t ch = iswalpha(g_apps[i].name[0]) ? (wchar_t)towupper(g_apps[i].name[0]) : L'#';
        if (ch != last) { wchar_t s[2] = { ch, 0 }; add_row(0, 0, L"", s, ""); last = ch; } add_row(1, 0, g_apps[i].path, g_apps[i].name, ""); }
}
static int row_h(const Row *r) { return r->kind == 0 ? px(30) : (r->kind == 3 ? px(36) : px(cfg_int("list_row_h"))); }
static int content_h(void) { int t = 0; for (int i = 0; i < g_nrows; i++) t += row_h(&g_rows[i]); return t; }

/* ------------------------------------------------------------------ tiles */
static void tile_layout(const Geo *g) {
    g_ntiles = 0; g_tile_content = 0; if (!g->has_pane) return;
    int cw = cell_w(), ch = cell_h(), gp = px(4); int avail = g->pane.right - g->pane.left - px(24); int cols = (avail + gp) / (cw + gp); if (cols < 1) cols = 1; if (cols > 16) cols = 16;
    static unsigned char occ[300][16]; memset(occ, 0, sizeof occ);
    int x0 = g->pane.left + px(12), y0 = g->pane.top + px(44), maxb = y0; JNode *pins = cfg_node("start_pins"), *sizes = cfg_node("tile_sizes"); if (!pins || pins->t != J_ARR) return;
    int flat = g->mode == LY_GRID;
    for (JNode *c = pins->child; c && g_ntiles < MAX_PINS; c = c->next) {
        if (!c->s) continue; JNode *sz = json_get(sizes, c->s); const char *size = flat ? "medium" : (sz && sz->s ? sz->s : "medium"); int sw_ = 1, sh_ = 1;
        if (!strcmp(size, "wide")) sw_ = 2; else if (!strcmp(size, "large")) { sw_ = 2; sh_ = 2; } if (sw_ > cols) sw_ = cols;
        int r = 0, col = 0, placed = 0;
        while (!placed && r < 280) { for (col = 0; col + sw_ <= cols && !placed; col++) { int free_ = 1;
                for (int i = 0; i < sh_ && free_; i++) for (int j = 0; j < sw_; j++) if (occ[r + i][col + j]) { free_ = 0; break; }
                if (free_) { for (int i = 0; i < sh_; i++) for (int j = 0; j < sw_; j++) occ[r + i][col + j] = 1; placed = 1; col--; } }
            if (!placed) r++; }
        Tile *t = &g_tiles[g_ntiles++]; w_from_utf8_buf(c->s, t->path, MAX_PATH); t->sw = sw_; t->sh = sh_;
        int x = x0 + col * (cw + gp), y = y0 + r * (ch + gp); t->r = R(x, y, x + sw_ * cw + (sw_ - 1) * gp, y + sh_ * ch + (sh_ - 1) * gp); if (t->r.bottom > maxb) maxb = t->r.bottom;
    }
    g_tile_content = maxb - y0 + px(12);
}
static RECT tile_screen(const Tile *t) { RECT r = t->r; r.top -= g_tile_scroll; r.bottom -= g_tile_scroll; return r; }
static int link_row_h(void) { return px(38); }
static int links_shown(int *idx, int max) {
    int n = 0; JNode *a = cfg_node("pane_items");
    for (JNode *c = a ? a->child : NULL; c && n < max; c = c->next) for (int i = 0; i < NLINKS; i++) if (c->s && !strcmp(c->s, LINKS[i].id)) idx[n++] = i;
    return n;
}
static void clamp_scroll(const Geo *g) {
    int m = content_h() - (g->list.bottom - g->list.top - g->list_top); if (m < 0) m = 0; if (g_scroll > m) g_scroll = m; if (g_scroll < 0) g_scroll = 0;
    int ph = g->pane.bottom - g->pane.top - px(44); int tc = g_tile_content; if (cfg_str("start_pane")[0] == 'l') { int ids[NLINKS]; tc = links_shown(ids, NLINKS) * link_row_h() + px(12); }
    m = tc - ph; if (m < 0) m = 0; if (g_tile_scroll > m) g_tile_scroll = m; if (g_tile_scroll < 0) g_tile_scroll = 0;
}
static COLORREF tile_color(int idx, const wchar_t *path, HICON ic, const Theme *th) {
    char *u = utf8_from_w(path); JNode *tc = json_get(cfg_node("tile_colors"), u); free(u); const char *mode = cfg_str("tile_color_mode");
    if (tc && tc->s && tc->s[0] && !strcmp(mode, "custom")) return c_hex(tc->s, th->accent);
    if (!strcmp(mode, "icon")) return c_shade(icon_accent(ic, th->accent), 0.55); if (!strcmp(mode, "accent")) return c_shade(th->accent, 0.8);
    return TILE_COLORS[(idx + g_shuffle_seed) % (int)(sizeof TILE_COLORS / sizeof TILE_COLORS[0])];
}
void shuffle_tiles(void) {
    srand(GetTickCount()); JNode *pins = cfg_node("start_pins"), *sizes = cfg_node("tile_sizes"), *cols = cfg_node("tile_colors"); if (!pins || !sizes || !cols) return;
    for (JNode *c = pins->child; c; c = c->next) { if (!c->s) continue; int r = rand() % 100;
        json_put(sizes, c->s, json_str(r < 55 ? "medium" : (r < 82 ? "wide" : "large")));
        COLORREF col = c_hsv(rand() % 360, 0.55 + (rand() % 30) / 100.0, 0.55 + (rand() % 35) / 100.0);
        char hx[8]; snprintf(hx, 8, "#%02X%02X%02X", GetRValue(col), GetGValue(col), GetBValue(col)); json_put(cols, c->s, json_str(hx)); }
    cfg_set_str("tile_color_mode", "custom");
}

/* ---------------------------------------------------------------- painting */
static void hl(HDC dc, RECT r, int glass, const Theme *th) {
    if (glass) { int dark = app_is_dark(); g_alpha(dc, r, dark ? RGB(255, 255, 255) : RGB(0, 0, 0), dark ? 36 : 26); } else g_fill(dc, r, th->hover);
}
static RECT sq(RECT r, int pct) { int s = (r.right - r.left < r.bottom - r.top ? r.right - r.left : r.bottom - r.top) * pct / 100; int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2; return R(cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2); }
static void draw_avatar(HDC dc, RECT r, const Theme *th) {
    int s = r.right - r.left < r.bottom - r.top ? r.right - r.left : r.bottom - r.top; int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2; RECT a = R(cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2);
    HRGN rgn = CreateEllipticRgn(a.left, a.top, a.right + 1, a.bottom + 1); wchar_t ph[MAX_PATH]; w_from_utf8_buf(cfg_str("user_photo"), ph, MAX_PATH);
    HBRUSH b = CreateSolidBrush(th->accent); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, a.left, a.top, a.right + 1, a.bottom + 1); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
    if (ph[0]) { SelectClipRgn(dc, rgn); img_draw(dc, ph, a, 0, 0); SelectClipRgn(dc, NULL); }
    else { wchar_t un[64]; w_from_utf8_buf(cfg_str("user_name"), un, 64); wchar_t init[2] = { un[0] ? (wchar_t)towupper(un[0]) : L'U', 0 }; HGDIOBJ o = SelectObject(dc, g_font_big); g_text(dc, init, a, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255)); SelectObject(dc, o); }
    DeleteObject(rgn);
}
static void draw_user(HDC dc, const Geo *g, const Theme *th) {
    if (g->user.right <= g->user.left) return; int av = px(cfg_int("avatar_size")); RECT u = g->user; int hot = g_hover_kind == HK_USER;
    if (hot) hl(dc, u, cfg_bool("glass"), th);
    draw_avatar(dc, R(u.left + px(4), (u.top + u.bottom) / 2 - av / 2, u.left + px(4) + av, (u.top + u.bottom) / 2 + av / 2), th);
    wchar_t un[64]; w_from_utf8_buf(cfg_str("user_name"), un, 64); g_text(dc, un, R(u.left + av + px(14), u.top, u.right, u.bottom), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th->text);
}
static void draw_search_box(HDC dc, const Geo *g, const Theme *th, int glass) {
    if (!g->has_search) return; RECT sr = g->search; if (glass) g_alpha(dc, sr, th->input, 205); else g_fill(dc, sr, th->input);
    g_frame(dc, sr, g_query[0] ? th->accent : th->active, 1); el_draw(dc, E_SM_SEARCH, R(sr.left + px(8), sr.top + px(6), sr.left + px(8) + (sr.bottom - sr.top - px(12)), sr.bottom - px(6)), th->dim, G_SEARCH);
}
static void draw_rows(HDC dc, RECT area, const Theme *th, int glass) {
    HRGN rgn = CreateRectRgn(area.left, area.top, area.right, area.bottom); SelectClipRgn(dc, rgn); int ry = area.top - g_scroll;
    for (int i = 0; i < g_nrows; i++) {
        Row *r = &g_rows[i]; int rh = row_h(r);
        if (ry + rh > area.top && ry < area.bottom) {
            int ind = px(r->level * 18);
            if (r->kind == 0) g_text(dc, r->name, R(area.left + px(16), ry, area.right, ry + rh), DT_VCENTER | DT_SINGLELINE, th->accent);
            else if (r->kind == 3) { RECT rr = R(area.left, ry, area.right - px(8), ry + rh); if (g_hover_kind == HK_GROUP && !_wcsicmp(g_hover_path, r->name)) hl(dc, rr, glass, th);
                glyph_draw(is_exp(r->group) ? G_CHEVRON_D : G_CHEVRON_R, dc, R(area.left + px(8) + ind, ry, area.left + px(28) + ind, ry + rh), th->dim);
                glyph_draw(G_FOLDER, dc, R(area.left + px(30) + ind, ry + px(8), area.left + px(50) + ind, ry + rh - px(8)), th->accent);
                g_text(dc, r->name, R(area.left + px(58) + ind, ry, area.right - px(14), ry + rh), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th->text); }
            else { RECT rr = R(area.left, ry, area.right - px(8), ry + rh);
                if (g_hover_kind == (r->kind == 2 ? HK_RUN : HK_APP) && !wcscmp(g_hover_path, r->path)) hl(dc, rr, glass, th);
                HICON ic = icon_for_file(r->kind == 2 ? L"cmd.exe" : r->path); int isz = px(24) < rh - 6 ? px(24) : rh - 6; DrawIconEx(dc, area.left + px(14) + ind, ry + (rh - isz) / 2, ic, isz, isz, 0, NULL, DI_NORMAL);
                g_text(dc, r->name, R(area.left + px(50) + ind, ry, area.right - px(14), ry + rh), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th->text); }
        }
        ry += rh;
    }
    SelectClipRgn(dc, NULL); DeleteObject(rgn);
    int ch = content_h(), vh = area.bottom - area.top; if (ch > vh) { int t = vh * vh / ch; if (t < px(24)) t = px(24); int ty = area.top + (int)((double)g_scroll / (ch - vh) * (vh - t)); g_alpha(dc, R(area.right - px(6), ty, area.right - px(2), ty + t), th->text, 90); }
}
static void link_rect(const Geo *g, int k, RECT *r) { int top = g->pane.top + px(44) + k * link_row_h() - g_tile_scroll; *r = R(g->pane.left + px(8), top, g->pane.right - px(8), top + link_row_h()); }
static void draw_links(HDC dc, const Geo *g, const Theme *th, int glass) {
    int ids[NLINKS], n = links_shown(ids, NLINKS); HRGN rgn = CreateRectRgn(g->pane.left, g->pane.top + px(44), g->pane.right, g->pane.bottom); SelectClipRgn(dc, rgn);
    for (int k = 0; k < n; k++) {
        const PaneLink *pl = &LINKS[ids[k]]; RECT r; link_rect(g, k, &r); if (r.bottom < g->pane.top + px(44) || r.top > g->pane.bottom) continue;
        JNode *cu = json_get(cfg_node("pane_custom"), pl->id); JNode *jl = cu ? json_get(cu, "label") : NULL, *ji = cu ? json_get(cu, "icon") : NULL, *jc = cu ? json_get(cu, "color") : NULL;
        wchar_t lab[100]; w_from_utf8_buf(jl && jl->s && jl->s[0] ? jl->s : pl->label, lab, 100); COLORREF col = jc && jc->s && jc->s[0] ? c_hex(jc->s, th->text) : th->text;
        if (g_hover_kind == HK_LINK && g_hover_path[0] == L'#' && (int)wcstol(g_hover_path + 1, NULL, 10) == k) hl(dc, r, glass, th);
        RECT ir = R(r.left + px(8), r.top + px(7), r.left + px(8) + link_row_h() - px(14), r.bottom - px(7)); int drawn = 0;
        if (ji && ji->s && ji->s[0]) { wchar_t w[MAX_PATH]; w_from_utf8_buf(ji->s, w, MAX_PATH); drawn = img_draw(dc, w, ir, col, jc && jc->s && jc->s[0]); }
        if (!drawn) glyph_draw(pl->glyph, dc, ir, jc && jc->s && jc->s[0] ? col : th->accent);
        g_text(dc, lab, R(ir.right + px(12), r.top, r.right - px(6), r.bottom), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, col);
    }
    SelectClipRgn(dc, NULL); DeleteObject(rgn);
}
static int show_rows(const Geo *g) { return g->mode == LY_GRID ? (g_grid_all || g_query[0]) : g->has_list; }
static RECT rows_area(const Geo *g) { return g->mode == LY_GRID ? g->pane : R(g->list.left, g->list.top + g->list_top, g->list.right, g->list.bottom); }

static void draw_tiles(HDC dc, const Geo *g, const Theme *th, int glass) {
    HRGN rgn = CreateRectRgn(g->pane.left, g->pane.top + px(44), g->pane.right, g->pane.bottom); SelectClipRgn(dc, rgn); int flat = g->mode == LY_GRID;
    for (int i = 0; i < g_ntiles; i++) {
        Tile *t = &g_tiles[i]; RECT r = tile_screen(t); if (r.bottom <= g->pane.top + px(44) || r.top >= g->pane.bottom) continue;
        HICON ic = icon_for_file(t->path); int hot = g_hover_kind == HK_TILE && !wcscmp(g_hover_path, t->path); int w = r.right - r.left, h = r.bottom - r.top;
        wchar_t nm[128]; display_name(t->path, nm, 128);
        if (flat) { if (hot) hl(dc, r, glass, th); int isz = (int)((w < h ? w : h) * 0.48); DrawIconEx(dc, r.left + (w - isz) / 2, r.top + px(6), ic, isz, isz, 0, NULL, DI_NORMAL);
            g_text(dc, nm, R(r.left + 2, r.top + px(8) + isz, r.right - 2, r.bottom - 2), DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th->text); continue; }
        COLORREF color = tile_color(i, t->path, ic, th); if (glass) g_alpha(dc, r, color, 225); else g_fill(dc, r, color);
        if (hot) g_frame(dc, r, RGB(255, 255, 255), 2);
        double f = t->sw == 1 && t->sh == 1 ? 0.42 : (t->sw == 2 && t->sh == 1 ? 0.38 : 0.34); int isz = (int)((w < h ? w : h) * f); if (isz < 16) isz = 16; if (isz > 96) isz = 96;
        int wide = t->sw == 2 && t->sh == 1; int ix = wide ? r.left + px(12) : r.left + (w - isz) / 2, iy = wide ? r.top + (h - isz) / 2 - px(6) : r.top + (int)(h * 0.14);
        DrawIconEx(dc, ix, iy, ic, isz, isz, 0, NULL, DI_NORMAL);
        g_text(dc, nm, R(r.left + px(6), r.bottom - px(26), r.right - px(4), r.bottom - px(4)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, RGB(255, 255, 255));
    }
    SelectClipRgn(dc, NULL); DeleteObject(rgn);
}

static void draw(HDC dc, int W, int H) {
    Theme th = app_theme(0); int glass = cfg_bool("glass"); Geo g; geometry(&g); tile_layout(&g); clamp_scroll(&g);
    if (glass) g_glass_blit(dc, 1, W, H, th.start_bg); else g_fill(dc, R(0, 0, W, H), th.start_bg);
    SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font);
    if (g.mode == LY_TILES || g.mode == LY_CLASSIC) {
        COLORREF rc = cfg_str("vbar_color")[0] ? c_hex(cfg_str("vbar_color"), th.rail) : th.rail;
        if (glass) g_alpha(dc, g.rail, rc, 255 * cfg_int("vbar_opacity") / 100 > 255 ? 255 : (cfg_int("vbar_opacity") > 0 ? 255 * cfg_int("vbar_opacity") / 100 : 255)); else g_fill(dc, g.rail, rc);
    }
    if (show_rows(&g)) draw_rows(dc, rows_area(&g), &th, glass);
    if (g.mode == LY_TILES || g.mode == LY_CLASSIC) {
        if (g.has_list) { draw_user(dc, &g, &th);
            if (g.add.right > g.add.left) { COLORREF col = g_hover_kind == HK_ADD ? th.accent : th.dim; HPEN pen = CreatePen(PS_SOLID, 2, col); HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                Ellipse(dc, g.add.left, g.add.top, g.add.right, g.add.bottom); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen); el_draw(dc, E_SM_ADD, sq(g.add, 55), th.text, G_PLUS); }
            draw_search_box(dc, &g, &th, glass); }
    } else if (g.mode == LY_TWOCOL) { draw_user(dc, &g, &th); draw_search_box(dc, &g, &th, glass); if (glass) g_alpha(dc, R(g.pane.left, 0, g.pane.left + 1, H), th.text, 40); }
    else { draw_search_box(dc, &g, &th, glass); draw_user(dc, &g, &th); }
    /* pane title + content */
    if (g.has_pane && !(g.mode == LY_GRID && show_rows(&g))) {
        int links = cfg_str("start_pane")[0] == 'l'; HGDIOBJ o = SelectObject(dc, g_font_big);
        g_text(dc, links ? L"Links" : L"Pinned", g.title, DT_VCENTER | DT_SINGLELINE, th.text); SelectObject(dc, o);
        if (links) draw_links(dc, &g, &th, glass); else draw_tiles(dc, &g, &th, glass);
    } else if (g.mode == LY_GRID && show_rows(&g)) { HGDIOBJ o = SelectObject(dc, g_font_big); g_text(dc, g_query[0] ? L"Search results" : L"All apps", g.title, DT_VCENTER | DT_SINGLELINE, th.text); SelectObject(dc, o); }
    if (g.sw.right > g.sw.left && g.has_pane) { if (g_hover_kind == HK_SW) hl(dc, g.sw, glass, &th); el_draw(dc, E_SM_SWITCH, sq(g.sw, 62), th.text, G_SWITCH); }
    if (g.mode == LY_GRID) { if (g_hover_kind == HK_ALL) hl(dc, g.all, glass, &th); g_text(dc, g_grid_all ? L"Back" : L"All apps >", g.all, DT_VCENTER | DT_CENTER | DT_SINGLELINE, th.text); }
    for (int i = 0; i < 6; i++) { if (g.ri[i].right <= g.ri[i].left) continue; wchar_t key[8]; _snwprintf(key, 8, L"#%d", i);
        int hot = g_hover_kind == HK_RAIL && !wcscmp(g_hover_path, key); if (hot || (i == 1 && cfg_bool("start_programs_mode"))) hl(dc, g.ri[i], glass, &th);
        el_draw(dc, RAIL_EL[i], sq(g.ri[i], 52), (i == 1 && cfg_bool("start_programs_mode")) ? th.accent : th.text, RAIL_GLYPH[i]); }
    if (g_edit_layout == 2) {
        const RECT *els[] = { &g.user, &g.add, &g.search, &g.sw }; for (int k = 0; k < 4; k++) if (els[k]->right > els[k]->left) { g_alpha(dc, *els[k], th.accent, 45); g_frame(dc, *els[k], th.accent, 1); }
        for (int i = 0; i < 6; i++) if (g.ri[i].right > g.ri[i].left) { g_alpha(dc, g.ri[i], th.accent, 45); g_frame(dc, g.ri[i], th.accent, 1); }
    }
    SelectObject(dc, of);
}

static void paint(void) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(g_hwnd, &ps); RECT rc; GetClientRect(g_hwnd, &rc); int W = rc.right, H = rc.bottom;
    HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H); HGDIOBJ old = SelectObject(mem, bmp);
    draw(mem, W, H); BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(g_hwnd, &ps);
}
static void invalidate(void) { if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }

/* ------------------------------------------------------------ interaction */
static int hit(int x, int y, int *aux, wchar_t *path) {
    Geo g; geometry(&g); tile_layout(&g); path[0] = 0; *aux = -1;
    for (int i = 0; i < 6; i++) if (g.ri[i].right > g.ri[i].left && in_rect(&g.ri[i], x, y)) { *aux = i; _snwprintf(path, MAX_PATH, L"#%d", i); return HK_RAIL; }
    if (g.user.right > g.user.left && in_rect(&g.user, x, y)) return HK_USER;
    if (g.add.right > g.add.left && in_rect(&g.add, x, y)) return HK_ADD;
    if (g.sw.right > g.sw.left && g.has_pane && in_rect(&g.sw, x, y)) return HK_SW;
    if (g.mode == LY_GRID && in_rect(&g.all, x, y)) return HK_ALL;
    if (g.has_search && in_rect(&g.search, x, y)) return HK_SEARCH;
    if (show_rows(&g)) { RECT a = rows_area(&g); if (in_rect(&a, x, y)) { int ry = a.top - g_scroll;
            for (int i = 0; i < g_nrows; i++) { int h = row_h(&g_rows[i]); if (y >= ry && y < ry + h && g_rows[i].kind != 0) { *aux = i; lstrcpyW(path, g_rows[i].kind == 3 ? g_rows[i].name : g_rows[i].path);
                    return g_rows[i].kind == 3 ? HK_GROUP : (g_rows[i].kind == 2 ? HK_RUN : HK_APP); } ry += h; } return HK_NONE; } }
    if (g.has_pane && in_rect(&g.pane, x, y) && y >= g.pane.top + px(44) && !(g.mode == LY_GRID && show_rows(&g))) {
        if (cfg_str("start_pane")[0] == 'l') { int ids[NLINKS], n = links_shown(ids, NLINKS); for (int k = 0; k < n; k++) { RECT r; link_rect(&g, k, &r); if (in_rect(&r, x, y)) { *aux = ids[k]; _snwprintf(path, MAX_PATH, L"#%d", k); return HK_LINK; } } }
        else for (int i = 0; i < g_ntiles; i++) { RECT r = tile_screen(&g_tiles[i]); if (in_rect(&r, x, y)) { *aux = i; lstrcpyW(path, g_tiles[i].path); return HK_TILE; } }
    }
    return HK_NONE;
}
static int kind_element(int kind, int aux) {
    switch (kind) { case HK_RAIL: return RAIL_EL[aux]; case HK_USER: return E_SM_USER; case HK_ADD: return E_SM_ADD; case HK_SW: return E_SM_SWITCH; case HK_SEARCH: return E_SM_SEARCH; default: return -1; }
}

void start_close(void) {
    if (!g_start_visible) return;
    g_start_visible = 0; g_closed_at = GetTickCount(); KillTimer(g_hwnd, 5); g_edit_layout = g_edit_layout == 2 ? 0 : g_edit_layout;
    if (cfg_bool("animations") && cfg_bool("anim_start_on")) {
        RECT cur; GetWindowRect(g_hwnd, &cur); const char *d = cfg_str("anim_start_dir"); if (!strcmp(d, "auto") || !d[0]) d = anim_edge_dir(cfg_str("position"));
        int base = cfg_bool("glass") ? 255 * cfg_int("glass_opacity") / 100 : -1;
        anim_run(g_hwnd, cur, anim_offset(cur, d, px(70)), cfg_int("anim_start_ms"), base, base < 0 ? -1 : 0, ANIM_END_HIDE, cur);
    } else ShowWindow(g_hwnd, SW_HIDE);
    bar_invalidate();
}

static void launch_cmd(const wchar_t *path) {
    wchar_t t[MAX_PATH]; lstrcpynW(t, path, MAX_PATH); wchar_t *p = t; while (*p == L' ') p++;
    if (*p == L'"') { wchar_t *e = wcschr(p + 1, L'"'); if (e) { *e = 0; app_run(p + 1, e[1] ? e + 1 : NULL, SW_SHOWNORMAL); return; } }
    if (path_exists(p)) { app_run(p, NULL, SW_SHOWNORMAL); return; }
    wchar_t *sp = wcschr(p, L' '); if (sp) { *sp = 0; app_run(p, sp + 1, SW_SHOWNORMAL); } else app_run(p, NULL, SW_SHOWNORMAL);
}
static void run_dialog(void) { wchar_t w[MAX_PATH]; find_exe(L"winerun.exe", w, MAX_PATH); app_run(path_exists(w) ? L"winerun.exe" : L"cmd.exe", NULL, SW_SHOWNORMAL); }
static void launch(const wchar_t *path, int as_cmd) {
    start_close();
    if (as_cmd) launch_cmd(path); else { char *u = utf8_from_w(path); app_record_use(u); free(u); app_run(path, NULL, SW_SHOWNORMAL); }
}
static void launch_link(int i) {
    const PaneLink *pl = &LINKS[i]; start_close();
    if (!strcmp(pl->id, "run")) { run_dialog(); return; } if (!strcmp(pl->id, "settings")) { settings_open(-1); return; }
    wchar_t t[MAX_PATH]; ExpandEnvironmentStringsW(pl->target, t, MAX_PATH);
    if (pl->params) app_run(t, pl->params, SW_SHOWNORMAL); else if (path_isdir(t)) app_run(L"explorer.exe", t, SW_SHOWNORMAL); else app_run(t, NULL, SW_SHOWNORMAL);
}
static void list_toggle(const char *key, const wchar_t *wpath) {
    char *u = utf8_from_w(wpath); JNode *a = cfg_node(key);
    if (a && a->t == J_ARR) {
        if (cfg_list_has(key, u)) { JNode *c = a->child, *prev = NULL; while (c) { JNode *nx = c->next; if (c->s && !strcmp(c->s, u)) { if (prev) prev->next = nx; else a->child = nx; c->next = NULL; json_free(c); } else prev = c; c = nx; } }
        else json_push(a, json_str(u));
        app_save();
    }
    free(u);
}
static void pick_app(void) {
    wchar_t file[MAX_PATH] = L""; OPENFILENAMEW o; memset(&o, 0, sizeof o); o.lStructSize = sizeof o; o.hwndOwner = g_hwnd; o.lpstrFile = file; o.nMaxFile = MAX_PATH;
    o.lpstrFilter = L"Programs (*.exe;*.lnk)\0*.exe;*.lnk\0All files\0*.*\0"; o.lpstrTitle = L"Add an app to the Start menu"; o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_menu_depth++; BOOL ok = GetOpenFileNameW(&o); g_menu_depth--; SetForegroundWindow(g_hwnd);
    if (ok) { char *u = utf8_from_w(file); JNode *a = cfg_node("start_apps"); if (a && !cfg_list_has("start_apps", u)) json_push(a, json_str(u)); free(u); app_save(); scan(); build_rows(); invalidate(); }
}
static void item_menu(int kind, const wchar_t *path) {
    if (kind == HK_RUN) return;
    char *u = utf8_from_w(path); int pinned_start = cfg_list_has("start_pins", u), custom = cfg_list_has("start_apps", u);
    JNode *sizes = cfg_node("tile_sizes"), *sz = json_get(sizes, u); const char *cur = sz && sz->s ? sz->s : "medium"; free(u);
    wchar_t ex[MAX_PATH], key[MAX_PATH]; exe_for(path, ex, MAX_PATH); app_key(ex, key, MAX_PATH);
    int pinned_bar = 0; JNode *pb = cfg_node("pinned");
    if (pb) for (JNode *c = pb->child; c; c = c->next) { wchar_t p[MAX_PATH], pe[MAX_PATH], pk[MAX_PATH]; w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe, MAX_PATH); app_key(pe, pk, MAX_PATH); if (!wcscmp(pk, key)) pinned_bar = 1; }
    const wchar_t *lab[12]; int id[12], fl[12], n = 0;
    #define ADD(i, t, f) do { lab[n] = (t); id[n] = (i); fl[n] = (f); n++; } while (0)
    ADD(1, L"Open", 0); ADD(2, pinned_start ? L"Unpin from Start" : L"Pin to Start", 0); ADD(3, pinned_bar ? L"Unpin from taskbar" : L"Pin to taskbar", 0); ADD(4, L"Open file location", 0);
    if (kind == HK_TILE) { ADD(-1, NULL, 0); ADD(11, L"Resize: Medium", !strcmp(cur, "medium") ? MF_CHECKED : 0); ADD(12, L"Resize: Wide", !strcmp(cur, "wide") ? MF_CHECKED : 0); ADD(13, L"Resize: Large", !strcmp(cur, "large") ? MF_CHECKED : 0); }
    if (custom) ADD(5, L"Remove from this list", 0);
    int cmd = app_popup(g_hwnd, lab, id, fl, n, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR);
    #undef ADD
    if (cmd == 1) launch(path, 0); else if (cmd == 2) list_toggle("start_pins", path);
    else if (cmd == 3) { if (pinned_bar) bar_unpin(key); else bar_pin(path); }
    else if (cmd == 4) { wchar_t prm[MAX_PATH + 16]; _snwprintf(prm, MAX_PATH + 16, L"/select,\"%ls\"", ex); app_run(L"explorer.exe", prm, SW_SHOWNORMAL); }
    else if (cmd == 5) { list_toggle("start_apps", path); scan(); build_rows(); }
    else if (cmd >= 11 && cmd <= 13) { char *k = utf8_from_w(path); json_put(sizes, k, json_str(cmd == 11 ? "medium" : cmd == 12 ? "wide" : "large")); free(k); app_save(); }
    invalidate();
}
static void power_menu(void) {
    const wchar_t *lab[] = { L"Restart taskbar", L"Exit taskbar", NULL, L"Shut down Wine session" }; int id[] = { 1, 2, -1, 3 }, fl[] = { 0, 0, 0, 0 };
    int cmd = app_popup(g_hwnd, lab, id, fl, 4, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR);
    if (cmd == 1) app_restart(); else if (cmd == 2) { start_close(); PostMessageW(bar_hwnd(), WM_CLOSE, 0, 0); } else if (cmd == 3) app_run(L"wineboot.exe", L"-s", SW_HIDE);
}
static void rail_action(int i) {
    switch (i) {
    case 0: cfg_set_bool("start_list_collapsed", !collapsed()); app_save(); start_apply(); break;
    case 1: cfg_set_bool("start_programs_mode", !cfg_bool("start_programs_mode")); app_save(); scan(); build_rows(); invalidate(); break;
    case 2: start_close(); app_run(L"control.exe", NULL, SW_SHOWNORMAL); break;
    case 3: start_close(); run_dialog(); break;
    case 4: start_close(); settings_open(-1); break;
    case 5: power_menu(); break;
    }
}
static void edit_menu(int e) {
    wchar_t t[80], l[60]; w_from_utf8_buf(el_label(e), l, 60); _snwprintf(t, 80, L"Hide: %ls", l); const wchar_t *lab[3] = { t, L"Reset position", L"Done editing" }; int id[3] = { 1, 2, 3 }, fl[3] = { 0, 0, 0 };
    int cmd = app_popup(g_hwnd, lab, id, fl, 3, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR);
    if (cmd == 1) { el_set_show(e, 0); app_save(); } else if (cmd == 2) { el_set_offset(e, 0, 0); app_save(); } else if (cmd == 3) g_edit_layout = 0; invalidate();
}
static void place_edit(void) {
    if (!g_edit) return; Geo g; geometry(&g);
    if (!g.has_search || g_edit_layout == 2) { if (cfg_bool("start_search_in_bar") && el_show(E_SM_SEARCH)) { SetWindowPos(g_edit, NULL, -700, -700, 100, 24, SWP_NOZORDER); ShowWindow(g_edit, SW_SHOWNA); } else ShowWindow(g_edit, SW_HIDE); return; }
    int fh = px(cfg_int("font_size")) + px(6), y = g.search.top + (g.search.bottom - g.search.top - fh) / 2;
    SetWindowPos(g_edit, NULL, g.search.left + px(34), y, g.search.right - g.search.left - px(44), fh, SWP_NOZORDER); ShowWindow(g_edit, SW_SHOW);
}
static void set_query(const wchar_t *t) { if (g_edit) SetWindowTextW(g_edit, t); lstrcpynW(g_query, t, 128); }
static void run_first(void) { for (int i = 0; i < g_nrows; i++) if (g_rows[i].kind == 1 || g_rows[i].kind == 2) { launch(g_rows[i].path, g_rows[i].kind == 2); return; } }

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN) {
        if (w == VK_RETURN) { run_first(); return 0; }
        if (w == VK_ESCAPE) { if (g_query[0]) { set_query(L""); build_rows(); invalidate(); bar_invalidate(); } else start_close(); return 0; }
        if (w == VK_UP || w == VK_DOWN) { g_scroll += (w == VK_UP ? -1 : 1) * px(40); if (g_scroll < 0) g_scroll = 0; invalidate(); return 0; }
    }
    if (m == WM_CHAR && (w == 13 || w == 27)) return 0;
    return CallWindowProcW(g_edit_old, h, m, w, l);
}

static LRESULT CALLBACK start_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int hd; LRESULT mr;
    switch (m) {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MEASUREITEM: case WM_DRAWITEM: mr = app_menu_msg(m, w, l, &hd); if (hd) return mr; break;
    case WM_CTLCOLOREDIT: { Theme th = app_theme(0); SetTextColor((HDC)w, th.text); SetBkColor((HDC)w, th.input);
        static HBRUSH br; static COLORREF last = (COLORREF)-1; if (last != th.input) { if (br) DeleteObject(br); br = CreateSolidBrush(th.input); last = th.input; } return (LRESULT)br; }
    case WM_COMMAND: if (LOWORD(w) == EDIT_ID && HIWORD(w) == EN_CHANGE) { wchar_t t[128]; GetWindowTextW(g_edit, t, 128); if (wcscmp(t, g_query)) { lstrcpynW(g_query, t, 128); build_rows(); invalidate(); bar_invalidate(); } } return 0;
    case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE && g_start_visible && g_menu_depth == 0 && g_edit_layout != 2) { HWND nw = (HWND)l; if (nw != g_edit && nw != settings_hwnd()) start_close(); } return 0;
    case WM_TIMER: if (w == 5 && g_start_visible) { g_shuffle_seed++; invalidate(); } return 0;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(l), y = (short)HIWORD(l);
        if (g_ed_el >= 0) { int du = x - g_ed_sx, dv = y - g_ed_sy; double sc = cfg_num("scale"); if (sc <= 0) sc = 1; el_set_offset(g_ed_el, g_ed_dx + (int)(du / sc), g_ed_dy + (int)(dv / sc)); place_edit(); invalidate(); return 0; }
        int aux; wchar_t p[MAX_PATH]; int k = hit(x, y, &aux, p);
        if (k != g_hover_kind || wcscmp(p, g_hover_path)) { g_hover_kind = k; lstrcpyW(g_hover_path, p); invalidate(); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); return 0; }
    case WM_MOUSELEAVE: g_hover_kind = HK_NONE; g_hover_path[0] = 0; invalidate(); return 0;
    case WM_MOUSEWHEEL: { int dl = (short)HIWORD(w); POINT pt = { (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &pt); Geo g; geometry(&g);
        int step = -dl / WHEEL_DELTA * px(60); if (g.has_pane && in_rect(&g.pane, pt.x, pt.y) && !(g.mode == LY_GRID && show_rows(&g))) g_tile_scroll += step; else g_scroll += step;
        tile_layout(&g); clamp_scroll(&g); invalidate(); return 0; }
    case WM_LBUTTONDOWN: if (g_edit_layout == 2) { int aux; wchar_t p[MAX_PATH]; int k = hit((short)LOWORD(l), (short)HIWORD(l), &aux, p); int e = kind_element(k, aux);
            if (e >= 0) { g_ed_el = e; g_ed_sx = (short)LOWORD(l); g_ed_sy = (short)HIWORD(l); el_offset(e, &g_ed_dx, &g_ed_dy); SetCapture(h); } }
        return 0;
    case WM_LBUTTONUP: {
        if (g_ed_el >= 0) { g_ed_el = -1; ReleaseCapture(); app_save(); return 0; }
        if (g_edit_layout == 2) return 0;
        int aux; wchar_t p[MAX_PATH]; int k = hit((short)LOWORD(l), (short)HIWORD(l), &aux, p);
        switch (k) {
        case HK_APP: launch(p, 0); break; case HK_RUN: launch(p, 1); break; case HK_TILE: launch(p, 0); break; case HK_LINK: launch_link(aux); break;
        case HK_GROUP: toggle_exp(g_rows[aux].group); { int keep = g_scroll; build_rows(); g_scroll = keep; } invalidate(); break;
        case HK_RAIL: rail_action(aux); break; case HK_ADD: pick_app(); break;
        case HK_USER: start_close(); settings_open_page("Startup and user"); break;
        case HK_SW: cfg_set_str("start_pane", cfg_str("start_pane")[0] == 'l' ? "tiles" : "links"); g_tile_scroll = 0; app_save(); invalidate(); break;
        case HK_ALL: g_grid_all = !g_grid_all; build_rows(); invalidate(); break;
        }
        return 0; }
    case WM_RBUTTONUP: { int aux; wchar_t p[MAX_PATH]; int k = hit((short)LOWORD(l), (short)HIWORD(l), &aux, p);
        if (g_edit_layout == 2) { int e = kind_element(k, aux); if (e >= 0) edit_menu(e); return 0; }
        if (k == HK_APP || k == HK_TILE) item_menu(k, p); return 0; }
    case WM_KEYDOWN: if (w == VK_ESCAPE) start_close(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void ensure_window(void) {
    if (g_hwnd) return;
    WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = start_proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLASS_NAME, L"WinShell Start", WS_POPUP, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, g_hwnd, (HMENU)(INT_PTR)EDIT_ID, wc.hInstance, NULL);
    if (g_edit) { g_edit_old = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)edit_proc); SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE); }
}
static void reshape(int move) {
    int x, y, W, H; position(&x, &y, &W, &H);
    if (move) SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, W, H, 0); else SetWindowPos(g_hwnd, NULL, 0, 0, W, H, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (cfg_bool("start_rounded") && cfg_bool("round_corners") && !is_full()) { int r = px(cfg_int("corner_radius")) * 2; SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, W + 1, H + 1, r, r), TRUE); } else SetWindowRgn(g_hwnd, NULL, TRUE);
    place_edit();
}
void start_apply(void) {
    if (!g_hwnd) return;
    if (g_edit) SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
    apply_glass(g_hwnd, cfg_bool("glass"), cfg_int("glass_opacity"));
    if (g_start_visible && !anim_busy(g_hwnd)) reshape(1); else { int x, y, W, H; position(&x, &y, &W, &H); (void)x; (void)y; SetWindowPos(g_hwnd, NULL, 0, 0, W, H, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); place_edit(); }
    invalidate();
}
void start_toggle(void) {
    if (g_start_visible) { start_close(); return; }
    if (GetTickCount() - g_closed_at < 250) return;
    ensure_window(); anim_cancel(g_hwnd); scan(); set_query(L""); g_grid_all = 0; build_rows(); g_tile_scroll = 0;
    apply_glass(g_hwnd, cfg_bool("glass"), cfg_int("glass_opacity")); reshape(0);
    int x, y, W, H; position(&x, &y, &W, &H); RECT fin = R(x, y, x + W, y + H); g_start_visible = 1;
    if (cfg_bool("animations") && cfg_bool("anim_start_on")) {
        const char *d = cfg_str("anim_start_dir"); if (!strcmp(d, "auto") || !d[0]) d = anim_edge_dir(cfg_str("position")); RECT from = anim_offset(fin, d, px(70));
        int base = cfg_bool("glass") ? 255 * cfg_int("glass_opacity") / 100 : -1;
        SetWindowPos(g_hwnd, HWND_TOPMOST, from.left, from.top, W, H, SWP_NOACTIVATE); if (base >= 0) SetLayeredWindowAttributes(g_hwnd, 0, 0, LWA_ALPHA);
        ShowWindow(g_hwnd, SW_SHOW); SetForegroundWindow(g_hwnd); anim_run(g_hwnd, from, fin, cfg_int("anim_start_ms"), base < 0 ? -1 : 0, base, ANIM_END_NONE, fin);
    } else { SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, W, H, 0); ShowWindow(g_hwnd, SW_SHOW); SetForegroundWindow(g_hwnd); }
    if (cfg_bool("tile_shuffle")) SetTimer(g_hwnd, 5, (UINT)cfg_int("tile_shuffle_seconds") * 1000, NULL);
    if (g_edit) SetFocus(g_edit);
    invalidate(); bar_invalidate();
}
void start_open_search(void) { if (!g_start_visible) start_toggle(); if (g_hwnd) { SetForegroundWindow(g_hwnd); if (g_edit) SetFocus(g_edit); } }
void start_destroy(void) { if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; g_edit = NULL; } }
