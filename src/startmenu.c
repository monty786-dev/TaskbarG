#include "app.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#define CLASS_NAME L"WinShellStart"
#define EDIT_ID 200
#define MAX_APPS 1024
#define MAX_ROWS 1300
#define MAX_PINS 64

typedef struct { wchar_t name[128]; wchar_t path[MAX_PATH]; } App;
typedef struct { int kind; wchar_t path[MAX_PATH]; wchar_t name[128]; } Row;   /* 0 head 1 app 2 run */

static HWND g_hwnd, g_edit;
int g_start_visible;
static DWORD g_closed_at;
static wchar_t g_query[128];
static App g_apps[MAX_APPS]; static int g_napps;
static Row g_rows[MAX_ROWS]; static int g_nrows;
static int g_scroll, g_tile_scroll, g_tile_content;
static int g_hover_kind; static wchar_t g_hover_path[MAX_PATH];     /* kinds: 1 app 2 run 3 tile 4 rail 5 add 6 user */
static WNDPROC g_edit_old;
static const wchar_t *g_rail_ids[] = { L"collapse", L"programs", L"system", L"run", L"settings", L"power" };

static const struct { const wchar_t *n, *e; } BUILTIN[] = {
    { L"Notepad", L"notepad.exe" }, { L"Command Prompt", L"cmd.exe" }, { L"Registry Editor", L"regedit.exe" },
    { L"Task Manager", L"taskmgr.exe" }, { L"Control Panel", L"control.exe" }, { L"File Explorer", L"explorer.exe" },
    { L"Wine Configuration", L"winecfg.exe" }, { L"WordPad", L"wordpad.exe" } };
static const COLORREF TILE_COLORS[] = {
    RGB(0,120,215), RGB(16,137,62), RGB(218,59,1), RGB(142,140,216), RGB(0,153,188), RGB(194,57,179), RGB(209,52,56),
    RGB(255,140,0), RGB(116,77,169), RGB(0,178,148), RGB(231,72,86), RGB(104,118,138), RGB(73,130,5), RGB(177,70,194), RGB(0,99,177) };

static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int collapsed(void) { return cfg_bool("start_list_collapsed"); }
static int is_full(void) { return !strcmp(cfg_str("start_size"), "full"); }
static int gap(void) { return is_full() ? 0 : px(8); }
HWND start_hwnd(void) { return g_hwnd; }

typedef struct { int rail, listw, tilesw, W, H; } Dims;
static Dims dims(void) {
    Dims d; int sw, sh; screen_size(&sw, &sh); RECT w = work_rect();
    d.rail = px(48); d.listw = collapsed() ? 0 : px(290);
    const char *size = cfg_str("start_size");
    if (is_full()) { d.tilesw = sw - d.rail - d.listw; d.W = sw; d.H = w.bottom - w.top; return d; }
    int large = !strcmp(size, "large");
    int tw = px(large ? 520 : 330), W = d.rail + d.listw + tw; if (W > sw - gap() * 2) W = sw - gap() * 2;
    int H = px(large ? 760 : 620); if (H > w.bottom - w.top - gap() * 2) H = w.bottom - w.top - gap() * 2;
    d.W = W; d.H = H; d.tilesw = W - d.rail - d.listw; return d;
}
static void position(int *x, int *y, int *W, int *H) {
    Dims d = dims(); int sw, sh; screen_size(&sw, &sh); int g = gap(); RECT w = work_rect();
    *W = d.W; *H = d.H;
    if (is_full()) { *x = 0; *y = w.top; return; }
    const char *al = cfg_str("alignment"); int xx = g;
    if (!strcmp(al, "center")) { xx = bar_start_left(); int mx = sw - d.W - g; if (xx > mx) xx = mx; if (xx < g) xx = g; }
    else if (!strcmp(al, "right")) { xx = bar_start_right() - d.W; int mx = sw - d.W - g; if (xx > mx) xx = mx; if (xx < g) xx = g; }
    *x = xx; *y = !strcmp(cfg_str("position"), "bottom") ? w.bottom - d.H - g : w.top + g;
}
static RECT search_rect(void) { Dims d = dims(); return R(d.rail + px(10), px(60), d.rail + d.listw - px(10), px(92)); }
static RECT add_rect(void) { Dims d = dims(); return R(d.rail + d.listw - px(42), px(12), d.rail + d.listw - px(10), px(44)); }
static int list_top(void) { return px(96); }
static int tiles_left(void) { Dims d = dims(); return d.rail + d.listw; }
static int tile_top(void) { return px(44); }
static int row_h(int kind) { return px(kind == 0 ? 30 : 40); }
static int content_h(void) { int t = 0; for (int i = 0; i < g_nrows; i++) t += row_h(g_rows[i].kind); return t; }
static void clamp_scroll(void) {
    Dims d = dims(); int m = content_h() - (d.H - list_top()); if (m < 0) m = 0;
    if (g_scroll > m) g_scroll = m; if (g_scroll < 0) g_scroll = 0;
    m = g_tile_content - (d.H - tile_top()); if (m < 0) m = 0;
    if (g_tile_scroll > m) g_tile_scroll = m; if (g_tile_scroll < 0) g_tile_scroll = 0;
}

/* -------------------------------------------------------------- app list */
static int cmp_app(const void *a, const void *b) { return _wcsicmp(((const App *)a)->name, ((const App *)b)->name); }

static int app_known(const wchar_t *name) {
    for (int i = 0; i < g_napps; i++) if (!_wcsicmp(g_apps[i].name, name)) return i;
    return -1;
}
static void app_set(const wchar_t *name, const wchar_t *path, int override) {
    int i = app_known(name);
    if (i >= 0) { if (override) lstrcpynW(g_apps[i].path, path, MAX_PATH); return; }
    if (g_napps >= MAX_APPS) return;
    lstrcpynW(g_apps[g_napps].name, name, 128); lstrcpynW(g_apps[g_napps].path, path, MAX_PATH); g_napps++;
}
static void walk(const wchar_t *dir, int depth) {
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; _snwprintf(pat, MAX_PATH, L"%ls\\*", dir);
    HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t full[MAX_PATH]; _snwprintf(full, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 4) walk(full, depth + 1); }
        else {
            size_t l = wcslen(fd.cFileName);
            if (l > 4 && !_wcsicmp(fd.cFileName + l - 4, L".lnk")) {
                wchar_t name[128]; lstrcpynW(name, fd.cFileName, 128); name[wcslen(name) - 4] = 0;
                if (_wcsnicmp(name, L"uninstall", 9) != 0) app_set(name, full, 0);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void scan(void) {
    wchar_t root[MAX_PATH], base[MAX_PATH]; g_napps = 0;
    DWORD n = GetEnvironmentVariableW(L"PROGRAMDATA", base, MAX_PATH); if (!n || n >= MAX_PATH) lstrcpyW(base, L"C:\\ProgramData");
    _snwprintf(root, MAX_PATH, L"%ls\\Microsoft\\Windows\\Start Menu\\Programs", base); walk(root, 0);
    n = GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH);
    if (n && n < MAX_PATH) { _snwprintf(root, MAX_PATH, L"%ls\\Microsoft\\Windows\\Start Menu\\Programs", base); walk(root, 0); }
    for (size_t i = 0; i < sizeof BUILTIN / sizeof BUILTIN[0]; i++) {
        wchar_t full[MAX_PATH]; find_exe(BUILTIN[i].e, full, MAX_PATH);
        if (path_exists(full)) app_set(BUILTIN[i].n, full, 0);
    }
    JNode *extra = cfg_node("start_apps");
    if (extra && extra->t == J_ARR) for (JNode *c = extra->child; c; c = c->next) {
        wchar_t p[MAX_PATH], nm[128]; w_from_utf8_buf(c->s, p, MAX_PATH); display_name(p, nm, 128); app_set(nm, p, 1);
    }
    qsort(g_apps, (size_t)g_napps, sizeof(App), cmp_app);
}
static const wchar_t *name_of(const wchar_t *path, wchar_t *buf) {
    for (int i = 0; i < g_napps; i++) if (!_wcsicmp(g_apps[i].path, path)) return g_apps[i].name;
    display_name(path, buf, 128); return buf;
}
static int path_known(const wchar_t *p) { for (int i = 0; i < g_napps; i++) if (!_wcsicmp(g_apps[i].path, p)) return 1; return 0; }

static void add_row(int kind, const wchar_t *path, const wchar_t *name) {
    if (g_nrows >= MAX_ROWS) return;
    g_rows[g_nrows].kind = kind; lstrcpynW(g_rows[g_nrows].path, path, MAX_PATH); lstrcpynW(g_rows[g_nrows].name, name, 128); g_nrows++;
}
typedef struct { wchar_t path[MAX_PATH]; int n; } Use;
static int cmp_use(const void *a, const void *b) { return ((const Use *)b)->n - ((const Use *)a)->n; }

static void build_rows(void) {
    wchar_t q[128]; lstrcpynW(q, g_query, 128); _wcslwr(q); wchar_t *qs = q; while (*qs == L' ') qs++;
    for (int e = (int)wcslen(qs); e > 0 && qs[e - 1] == L' '; ) qs[--e] = 0;
    g_nrows = 0; g_scroll = 0; wchar_t nb[128];
    if (qs[0]) {
        int idx[MAX_APPS], n = 0;
        for (int i = 0; i < g_napps; i++) { wchar_t l[128]; lstrcpyW(l, g_apps[i].name); _wcslwr(l); if (wcsstr(l, qs)) idx[n++] = i; }
        for (int pass = 0; pass < 2; pass++)           /* prefix matches first */
            for (int k = 0; k < n; k++) {
                wchar_t l[128]; lstrcpyW(l, g_apps[idx[k]].name); _wcslwr(l);
                int pre = wcsncmp(l, qs, wcslen(qs)) == 0;
                if ((pass == 0) == pre) add_row(1, g_apps[idx[k]].path, g_apps[idx[k]].name);
            }
        wchar_t label[160]; _snwprintf(label, 160, L"Run \"%ls\"", g_query); add_row(2, g_query, label);
        return;
    }
    if (cfg_bool("start_show_recent")) {
        JNode *rec = cfg_node("app_recent"); int shown = 0;
        if (rec && rec->t == J_ARR) for (JNode *c = rec->child; c && shown < 3; c = c->next) {
            wchar_t p[MAX_PATH]; w_from_utf8_buf(c->s, p, MAX_PATH);
            if (!path_known(p)) continue;
            if (!shown) add_row(0, L"", L"Recently used");
            add_row(1, p, name_of(p, nb)); shown++;
        }
    }
    if (cfg_bool("start_show_most_used")) {
        JNode *use = cfg_node("app_usage"); Use u[128]; int n = 0;
        if (use && use->t == J_OBJ) for (JNode *c = use->child; c && n < 128; c = c->next) {
            wchar_t p[MAX_PATH]; w_from_utf8_buf(c->key, p, MAX_PATH);
            if (c->n > 0 && path_known(p)) { lstrcpyW(u[n].path, p); u[n].n = (int)c->n; n++; }
        }
        qsort(u, (size_t)n, sizeof(Use), cmp_use);
        if (n > 4) n = 4;
        if (n) add_row(0, L"", L"Most used");
        for (int i = 0; i < n; i++) add_row(1, u[i].path, name_of(u[i].path, nb));
    }
    wchar_t last = 0;
    for (int i = 0; i < g_napps; i++) {
        wchar_t ch = iswalpha(g_apps[i].name[0]) ? (wchar_t)towupper(g_apps[i].name[0]) : L'#';
        if (ch != last) { wchar_t s[2] = { ch, 0 }; add_row(0, L"", s); last = ch; }
        add_row(1, g_apps[i].path, g_apps[i].name);
    }
}

/* ------------------------------------------------------------------ tiles */
typedef struct { wchar_t path[MAX_PATH]; RECT r; int span_w, span_h; } Tile;
static Tile g_tiles[MAX_PINS]; static int g_ntiles;

static void tile_layout(void) {
    Dims d = dims(); int cell = px(cfg_int("tile_size")), g = px(4);
    int avail = d.W - tiles_left() - px(24); int cols = (avail + g) / (cell + g); if (cols < 1) cols = 1;
    static unsigned char occ[256][16]; memset(occ, 0, sizeof occ); if (cols > 16) cols = 16;
    int x0 = tiles_left() + px(12), y0 = tile_top(), maxb = y0; g_ntiles = 0;
    JNode *pins = cfg_node("start_pins"), *sizes = cfg_node("tile_sizes");
    if (!pins || pins->t != J_ARR) return;
    for (JNode *c = pins->child; c && g_ntiles < MAX_PINS; c = c->next) {
        if (!c->s) continue;
        JNode *sz = json_get(sizes, c->s); const char *size = sz && sz->s ? sz->s : "medium";
        int sw_ = 1, sh_ = 1; if (!strcmp(size, "wide")) sw_ = 2; else if (!strcmp(size, "large")) { sw_ = 2; sh_ = 2; }
        if (sw_ > cols) sw_ = cols;
        int r = 0, col = 0, placed = 0;
        while (!placed && r < 250) {
            for (col = 0; col + sw_ <= cols && !placed; col++) {
                int free_ = 1;
                for (int i = 0; i < sh_ && free_; i++) for (int j = 0; j < sw_; j++) if (occ[r + i][col + j]) { free_ = 0; break; }
                if (free_) { for (int i = 0; i < sh_; i++) for (int j = 0; j < sw_; j++) occ[r + i][col + j] = 1; placed = 1; col--; }
            }
            if (!placed) r++;
        }
        Tile *t = &g_tiles[g_ntiles++]; w_from_utf8_buf(c->s, t->path, MAX_PATH);
        t->span_w = sw_; t->span_h = sh_;
        int x = x0 + col * (cell + g), y = y0 + r * (cell + g);
        t->r = R(x, y, x + sw_ * cell + (sw_ - 1) * g, y + sh_ * cell + (sh_ - 1) * g);
        if (t->r.bottom > maxb) maxb = t->r.bottom;
    }
    g_tile_content = maxb - y0 + px(12);
}
static RECT tile_screen_rect(const Tile *t) { RECT r = t->r; r.top -= g_tile_scroll; r.bottom -= g_tile_scroll; return r; }

static COLORREF tile_color(int idx, HICON ic, const Theme *th) {
    const char *mode = cfg_str("tile_color_mode");
    if (!strcmp(mode, "icon")) return c_shade(icon_accent(ic, th->accent), 0.55);
    if (!strcmp(mode, "accent")) return c_shade(th->accent, 0.8);
    return TILE_COLORS[idx % (int)(sizeof TILE_COLORS / sizeof TILE_COLORS[0])];
}

/* ---------------------------------------------------------------- painting */
static void hl(HDC dc, RECT r, int glass, const Theme *th) {
    if (glass) { int dark = app_is_dark(); g_alpha(dc, r, dark ? RGB(255, 255, 255) : RGB(0, 0, 0), dark ? 36 : 26); }
    else g_fill(dc, r, th->hover);
}
static void draw_rail_glyph(HDC dc, const wchar_t *id, RECT r, COLORREF c) {
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, s = px(9);
    HPEN pen = CreatePen(PS_SOLID, 2, c); HGDIOBJ op = SelectObject(dc, pen); HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    if (!wcscmp(id, L"collapse")) { for (int k = -4; k <= 4; k += 4) g_fill(dc, R(cx - s, cy + k - 1, cx + s, cy + k + 1), c); }
    else if (!wcscmp(id, L"programs")) { for (int k = -3; k <= 3; k += 3) g_fill(dc, R(cx - s, cy + k - 1, cx + s, cy + k + 1), c); }
    else if (!wcscmp(id, L"system")) Ellipse(dc, cx - s + 2, cy - s + 2, cx + s - 2, cy + s - 2);
    else if (!wcscmp(id, L"run")) { g_frame(dc, R(cx - s, cy - s, cx + s, cy + s), c, 1); MoveToEx(dc, cx - s + 2, cy - 2, NULL); LineTo(dc, cx - 2, cy + 2); LineTo(dc, cx - s + 2, cy + 6); }
    else if (!wcscmp(id, L"settings")) { Ellipse(dc, cx - s, cy - s, cx + s, cy + s); Ellipse(dc, cx - 3, cy - 3, cx + 3, cy + 3); }
    else if (!wcscmp(id, L"power")) { Arc(dc, cx - s, cy - s + 2, cx + s, cy + s + 2, cx + 4, cy - s + 3, cx - 4, cy - s + 3); MoveToEx(dc, cx, cy - s, NULL); LineTo(dc, cx, cy); }
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
}

static void draw(HDC dc, int W, int H) {
    Theme th = app_theme(0); int glass = cfg_bool("glass"); Dims d = dims(); int x, y, w_, h_;
    position(&x, &y, &w_, &h_);
    if (glass) g_glass_blit(dc, 1, W, H, th.start_bg); else g_fill(dc, R(0, 0, W, H), th.start_bg);
    SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font);
    tile_layout(); clamp_scroll();
    int rail = d.rail, lt = list_top();

    if (d.listw) {
        HRGN rgn = CreateRectRgn(rail, lt, rail + d.listw, H); SelectClipRgn(dc, rgn);
        int ry = lt - g_scroll;
        for (int i = 0; i < g_nrows; i++) {
            Row *r = &g_rows[i]; int rh = row_h(r->kind);
            if (ry + rh > lt && ry < H) {
                if (r->kind == 0) g_text(dc, r->name, R(rail + px(16), ry, rail + d.listw, ry + rh), DT_VCENTER | DT_SINGLELINE, th.accent);
                else {
                    if (g_hover_kind == r->kind && !wcscmp(g_hover_path, r->path)) hl(dc, R(rail, ry, rail + d.listw - px(8), ry + rh), glass, &th);
                    HICON ic = icon_for_file(r->kind == 2 ? L"cmd.exe" : r->path); int isz = px(24);
                    DrawIconEx(dc, rail + px(14), ry + (rh - isz) / 2, ic, isz, isz, 0, NULL, DI_NORMAL);
                    g_text(dc, r->name, R(rail + px(50), ry, rail + d.listw - px(14), ry + rh), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th.text);
                }
            }
            ry += rh;
        }
        SelectClipRgn(dc, NULL); DeleteObject(rgn);
        /* scrollbar thumb */
        int ch = content_h(), vh = H - lt;
        if (ch > vh) {
            int th_ = vh * vh / ch; if (th_ < px(24)) th_ = px(24);
            int ty = lt + (int)((double)g_scroll / (ch - vh) * (vh - th_));
            g_alpha(dc, R(rail + d.listw - px(6), ty, rail + d.listw - px(2), ty + th_), th.text, 90);
        }
        /* user header */
        if (g_hover_kind == 6) g_alpha(dc, R(rail, 0, rail + d.listw, px(56)), RGB(255, 255, 255), 24);
        int ar = px(20), cx = rail + px(34), cy = px(28);
        HBRUSH b = CreateSolidBrush(th.accent); HGDIOBJ ob = SelectObject(dc, b); HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, cx - ar, cy - ar, cx + ar, cy + ar); SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(b);
        wchar_t un[64]; w_from_utf8_buf(cfg_str("user_name"), un, 64); wchar_t init[2] = { un[0] ? (wchar_t)towupper(un[0]) : L'U', 0 };
        HGDIOBJ of2 = SelectObject(dc, g_font_big);
        g_text(dc, init, R(cx - ar, cy - ar, cx + ar, cy + ar), DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
        g_text(dc, un, R(rail + px(64), 0, rail + d.listw - px(52), px(56)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, th.text);
        SelectObject(dc, of2);
        RECT ad = add_rect(); int acx = (ad.left + ad.right) / 2, acy = (ad.top + ad.bottom) / 2;
        COLORREF col = g_hover_kind == 5 ? th.accent : th.dim; HPEN pen = CreatePen(PS_SOLID, 2, col); op = SelectObject(dc, pen); ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, acx - px(13), acy - px(13), acx + px(13), acy + px(13)); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
        g_fill(dc, R(acx - px(6), acy - 1, acx + px(7), acy + 1), th.text); g_fill(dc, R(acx - 1, acy - px(6), acx + 1, acy + px(7)), th.text);
        RECT sr = search_rect();
        if (glass) g_alpha(dc, sr, th.input, 205); else g_fill(dc, sr, th.input);
        g_frame(dc, sr, g_query[0] ? th.accent : th.active, 1);
        HPEN sp = CreatePen(PS_SOLID, 2, th.dim); op = SelectObject(dc, sp); ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        int mx = sr.left + px(14), my = (sr.top + sr.bottom) / 2 - 1;
        Ellipse(dc, mx - px(5), my - px(5), mx + px(4), my + px(4)); MoveToEx(dc, mx + px(3), my + px(3), NULL); LineTo(dc, mx + px(7), my + px(7));
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(sp);
    }
    /* rail */
    if (glass) g_alpha(dc, R(0, 0, rail, H), th.rail, 255 * cfg_int("vbar_opacity") / 100 > 255 ? 255 : 255 * cfg_int("vbar_opacity") / 100);
    else g_fill(dc, R(0, 0, rail, H), th.rail);
    for (int i = 0; i < 6; i++) {
        RECT r = R(0, i * rail, rail, (i + 1) * rail);
        if (g_hover_kind == 4 && !wcscmp(g_hover_path, g_rail_ids[i])) hl(dc, r, glass, &th);
        draw_rail_glyph(dc, g_rail_ids[i], r, th.text);
    }
    /* tiles */
    int tl = tiles_left(); g_text(dc, L"Pinned", R(tl + px(12), px(10), W - px(8), px(40)), DT_VCENTER | DT_SINGLELINE, th.text);
    if (!cfg_bool("tb_collapse_pinned")) {
        HRGN rgn = CreateRectRgn(tl, tile_top(), W, H); SelectClipRgn(dc, rgn);
        for (int i = 0; i < g_ntiles; i++) {
            Tile *t = &g_tiles[i]; RECT r = tile_screen_rect(t);
            if (r.bottom <= tile_top() || r.top >= H) continue;
            HICON ic = icon_for_file(t->path); COLORREF color = tile_color(i, ic, &th);
            if (glass) g_alpha(dc, r, color, 225); else g_fill(dc, r, color);
            if (g_hover_kind == 3 && !wcscmp(g_hover_path, t->path)) g_frame(dc, r, RGB(255, 255, 255), 2);
            int w = r.right - r.left, h = r.bottom - r.top;
            double f = t->span_w == 1 && t->span_h == 1 ? 0.42 : (t->span_w == 2 && t->span_h == 1 ? 0.38 : 0.34);
            int isz = (int)((w < h ? w : h) * f); if (isz < 16) isz = 16; if (isz > 96) isz = 96;
            int ix = t->span_w == 2 && t->span_h == 1 ? r.left + px(12) : r.left + (w - isz) / 2;
            int iy = t->span_w == 2 && t->span_h == 1 ? r.top + (h - isz) / 2 - px(6) : r.top + (int)(h * 0.14);
            DrawIconEx(dc, ix, iy, ic, isz, isz, 0, NULL, DI_NORMAL);
            wchar_t nm[128]; display_name(t->path, nm, 128);
            g_text(dc, nm, R(r.left + px(6), r.bottom - px(26), r.right - px(4), r.bottom - px(4)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, RGB(255, 255, 255));
        }
        SelectClipRgn(dc, NULL); DeleteObject(rgn);
    }
    SelectObject(dc, of); (void)x; (void)y; (void)w_; (void)h_;
}

static void paint(void) {
    PAINTSTRUCT ps; HDC hdc = BeginPaint(g_hwnd, &ps); Dims d = dims();
    HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, d.W, d.H); HGDIOBJ old = SelectObject(mem, bmp);
    draw(mem, d.W, d.H); BitBlt(hdc, 0, 0, d.W, d.H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(g_hwnd, &ps);
}
static void invalidate(void) { if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }

/* ------------------------------------------------------------ interaction */
static void place_edit(void) {
    if (!g_edit) return;
    if (collapsed()) { ShowWindow(g_edit, SW_HIDE); return; }
    RECT sr = search_rect(); int fh = px(cfg_int("font_size")) + px(6), y = sr.top + (sr.bottom - sr.top - fh) / 2;
    SetWindowPos(g_edit, NULL, sr.left + px(30), y, sr.right - sr.left - px(40), fh, SWP_NOZORDER); ShowWindow(g_edit, SW_SHOW);
}
static void set_edit_text(const wchar_t *t) { if (g_edit) SetWindowTextW(g_edit, t); lstrcpynW(g_query, t, 128); }

void start_close(void) {
    if (!g_start_visible) return;
    g_start_visible = 0; g_closed_at = GetTickCount(); ShowWindow(g_hwnd, SW_HIDE); bar_invalidate();
}

static void launch(const wchar_t *path, int as_cmd) {
    start_close();
    if (as_cmd) {
        wchar_t t[MAX_PATH]; lstrcpynW(t, path, MAX_PATH); wchar_t *p = t; while (*p == L' ') p++;
        if (*p == L'"') { wchar_t *e = wcschr(p + 1, L'"'); if (e) { *e = 0; app_run(p + 1, e[1] ? e + 1 : NULL, SW_SHOWNORMAL); return; } }
        if (path_exists(p)) { app_run(p, NULL, SW_SHOWNORMAL); return; }
        wchar_t *sp = wcschr(p, L' '); if (sp) { *sp = 0; app_run(p, sp + 1, SW_SHOWNORMAL); } else app_run(p, NULL, SW_SHOWNORMAL);
    } else { char *u = utf8_from_w(path); app_record_use(u); free(u); app_run(path, NULL, SW_SHOWNORMAL); }
}

static void list_toggle(const char *key, const wchar_t *wpath) {
    char *u = utf8_from_w(wpath); JNode *a = cfg_node(key);
    if (a && a->t == J_ARR) {
        if (cfg_list_has(key, u)) {
            JNode *c = a->child, *prev = NULL;
            while (c) { JNode *nx = c->next; if (c->s && !strcmp(c->s, u)) { if (prev) prev->next = nx; else a->child = nx; c->next = NULL; json_free(c); } else prev = c; c = nx; }
        } else json_push(a, json_str(u));
        app_save();
    }
    free(u);
}

static void pick_app(void) {
    wchar_t file[MAX_PATH] = L""; OPENFILENAMEW o; memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o; o.hwndOwner = g_hwnd; o.lpstrFile = file; o.nMaxFile = MAX_PATH;
    o.lpstrFilter = L"Programs (*.exe;*.lnk)\0*.exe;*.lnk\0All files\0*.*\0"; o.lpstrTitle = L"Add an app to the Start menu";
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_menu_depth++; BOOL ok = GetOpenFileNameW(&o); g_menu_depth--; SetForegroundWindow(g_hwnd);
    if (ok) { char *u = utf8_from_w(file); JNode *a = cfg_node("start_apps"); if (a && !cfg_list_has("start_apps", u)) json_push(a, json_str(u)); free(u); app_save(); scan(); build_rows(); invalidate(); }
}

static void item_menu(int kind, const wchar_t *path) {
    if (kind == 2) return;
    char *u = utf8_from_w(path); int pinned_start = cfg_list_has("start_pins", u), custom = cfg_list_has("start_apps", u); free(u);
    wchar_t ex[MAX_PATH], key[MAX_PATH]; exe_for(path, ex, MAX_PATH); app_key(ex, key, MAX_PATH);
    int pinned_bar = 0; JNode *pb = cfg_node("pinned");
    if (pb) for (JNode *c = pb->child; c; c = c->next) { wchar_t p[MAX_PATH], pe[MAX_PATH], pk[MAX_PATH]; w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe, MAX_PATH); app_key(pe, pk, MAX_PATH); if (!wcscmp(pk, key)) pinned_bar = 1; }
    const wchar_t *lab[10]; int id[10], fl[10], n = 0;
    #define ADD(i, t, f) do { lab[n] = (t); id[n] = (i); fl[n] = (f); n++; } while (0)
    char *up = utf8_from_w(path); JNode *sizes = cfg_node("tile_sizes"); JNode *sz = json_get(sizes, up); free(up);
    const char *cur = sz && sz->s ? sz->s : "medium";
    ADD(1, L"Open", 0); ADD(2, pinned_start ? L"Unpin from Start" : L"Pin to Start", 0);
    ADD(3, pinned_bar ? L"Unpin from taskbar" : L"Pin to taskbar", 0); ADD(4, L"Open file location", 0);
    if (kind == 3) { ADD(-1, NULL, 0); ADD(11, L"Resize: Medium", !strcmp(cur, "medium") ? MF_CHECKED : 0);
        ADD(12, L"Resize: Wide", !strcmp(cur, "wide") ? MF_CHECKED : 0); ADD(13, L"Resize: Large", !strcmp(cur, "large") ? MF_CHECKED : 0); }
    if (custom) ADD(5, L"Remove from this list", 0);
    int cmd = app_popup(g_hwnd, lab, id, fl, n, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR);
    #undef ADD
    if (cmd == 1) launch(path, 0);
    else if (cmd == 2) list_toggle("start_pins", path);
    else if (cmd == 3) { if (pinned_bar) bar_unpin(key); else bar_pin(path); }
    else if (cmd == 4) { wchar_t prm[MAX_PATH + 16]; _snwprintf(prm, MAX_PATH + 16, L"/select,\"%ls\"", ex); app_run(L"explorer.exe", prm, SW_SHOWNORMAL); }
    else if (cmd == 5) { list_toggle("start_apps", path); scan(); build_rows(); }
    else if (cmd >= 11 && cmd <= 13) { char *k = utf8_from_w(path); json_put(sizes, k, json_str(cmd == 11 ? "medium" : cmd == 12 ? "wide" : "large")); free(k); app_save(); }
    invalidate();
}

static void power_menu(void) {
    const wchar_t *lab[] = { L"Restart taskbar", L"Exit taskbar", NULL, L"Shut down Wine session" }; int id[] = { 1, 2, -1, 3 }, fl[] = { 0, 0, 0, 0 };
    int cmd = app_popup(g_hwnd, lab, id, fl, 4, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR);
    if (cmd == 1) app_restart(); else if (cmd == 2) { start_close(); PostMessageW(bar_hwnd(), WM_CLOSE, 0, 0); }
    else if (cmd == 3) app_run(L"wineboot.exe", L"-s", SW_HIDE);
}

static void rail_action(const wchar_t *id) {
    if (!wcscmp(id, L"settings")) { start_close(); wchar_t prm[MAX_PATH + 4]; _snwprintf(prm, MAX_PATH + 4, L"\"%ls\"", g_config_path); app_run(L"notepad.exe", prm, SW_SHOWNORMAL); }
    else if (!wcscmp(id, L"power")) power_menu();
    else if (!wcscmp(id, L"collapse")) { cfg_set_bool("start_list_collapsed", !collapsed()); app_save(); start_apply(); }
    else if (!wcscmp(id, L"run")) { start_close(); wchar_t w[MAX_PATH]; find_exe(L"winerun.exe", w, MAX_PATH); app_run(path_exists(w) ? L"winerun.exe" : L"cmd.exe", NULL, SW_SHOWNORMAL); }
    else if (!wcscmp(id, L"programs")) { start_close(); app_run(L"explorer.exe", L"shell:Programs", SW_SHOWNORMAL); }
    else if (!wcscmp(id, L"system")) { start_close(); app_run(L"control.exe", NULL, SW_SHOWNORMAL); }
}

static int hit(int x, int y, wchar_t *path) {
    Dims d = dims(); path[0] = 0;
    for (int i = 0; i < 6; i++) if (x >= 0 && x < d.rail && y >= i * d.rail && y < (i + 1) * d.rail) { lstrcpyW(path, g_rail_ids[i]); return 4; }
    if (d.listw && x >= d.rail && x < d.rail + d.listw) {
        if (y < px(56)) { RECT a = add_rect(); return in_rect(&a, x, y) ? 5 : 6; }
        if (y < list_top()) return 0;
        int ry = list_top() - g_scroll;
        for (int i = 0; i < g_nrows; i++) { int h = row_h(g_rows[i].kind);
            if (y >= ry && y < ry + h && g_rows[i].kind != 0) { lstrcpyW(path, g_rows[i].path); return g_rows[i].kind; } ry += h; }
        return 0;
    }
    if (x >= tiles_left() && y >= tile_top() && !cfg_bool("tb_collapse_pinned"))
        for (int i = 0; i < g_ntiles; i++) { RECT r = tile_screen_rect(&g_tiles[i]); if (in_rect(&r, x, y)) { lstrcpyW(path, g_tiles[i].path); return 3; } }
    return 0;
}

static void run_first(void) { for (int i = 0; i < g_nrows; i++) if (g_rows[i].kind == 1 || g_rows[i].kind == 2) { launch(g_rows[i].path, g_rows[i].kind == 2); return; } }

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN) {
        if (w == VK_RETURN) { run_first(); return 0; }
        if (w == VK_ESCAPE) { if (g_query[0]) { set_edit_text(L""); build_rows(); invalidate(); } else start_close(); return 0; }
        if (w == VK_UP || w == VK_DOWN) { g_scroll += (w == VK_UP ? -1 : 1) * px(40); clamp_scroll(); invalidate(); return 0; }
    }
    if (m == WM_CHAR && (w == 13 || w == 27)) return 0;
    return CallWindowProcW(g_edit_old, h, m, w, l);
}

static LRESULT CALLBACK start_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLOREDIT: { Theme th = app_theme(0); SetTextColor((HDC)w, th.text); SetBkColor((HDC)w, th.input);
        static HBRUSH br; static COLORREF last = (COLORREF)-1; if (last != th.input) { if (br) DeleteObject(br); br = CreateSolidBrush(th.input); last = th.input; } return (LRESULT)br; }
    case WM_COMMAND: if (LOWORD(w) == EDIT_ID && HIWORD(w) == EN_CHANGE) { wchar_t t[128]; GetWindowTextW(g_edit, t, 128); if (wcscmp(t, g_query)) { lstrcpynW(g_query, t, 128); build_rows(); invalidate(); } } return 0;
    case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE && g_start_visible && g_menu_depth == 0) { HWND nw = (HWND)l; if (nw != g_edit) start_close(); } return 0;
    case WM_MOUSEMOVE: {
        wchar_t p[MAX_PATH]; int k = hit((short)LOWORD(l), (short)HIWORD(l), p);
        if (k != g_hover_kind || wcscmp(p, g_hover_path)) { g_hover_kind = k; lstrcpyW(g_hover_path, p); invalidate(); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); return 0; }
    case WM_MOUSELEAVE: g_hover_kind = 0; g_hover_path[0] = 0; invalidate(); return 0;
    case WM_MOUSEWHEEL: { int dl = (short)HIWORD(w); POINT pt = { (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &pt);
        int step = -dl / WHEEL_DELTA * px(60); if (pt.x >= tiles_left()) g_tile_scroll += step; else g_scroll += step; clamp_scroll(); invalidate(); return 0; }
    case WM_LBUTTONUP: {
        wchar_t p[MAX_PATH]; int k = hit((short)LOWORD(l), (short)HIWORD(l), p);
        if (k == 1 || k == 3) launch(p, 0); else if (k == 2) launch(p, 1); else if (k == 4) rail_action(p); else if (k == 5) pick_app();
        else if (k == 0 && (short)LOWORD(l) >= tiles_left() && (short)HIWORD(l) < px(44) && (short)LOWORD(l) > dims().W - px(36)) { cfg_set_bool("tb_collapse_pinned", !cfg_bool("tb_collapse_pinned")); app_save(); invalidate(); }
        return 0; }
    case WM_RBUTTONUP: { wchar_t p[MAX_PATH]; int k = hit((short)LOWORD(l), (short)HIWORD(l), p); if (k == 1 || k == 3) item_menu(k, p); return 0; }
    case WM_KEYDOWN: if (w == VK_ESCAPE) start_close(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void ensure_window(void) {
    if (g_hwnd) return;
    WNDCLASSW wc; memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = start_proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, CLASS_NAME, L"WinShell Start", WS_POPUP, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, g_hwnd, (HMENU)(INT_PTR)EDIT_ID, wc.hInstance, NULL);
    if (g_edit) { g_edit_old = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)edit_proc); SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE); SendMessageW(g_edit, 0x1501 /*EM_SETCUEBANNER*/, 1, (LPARAM)L"Type here to search"); }
}

static void reshape(void) {
    int x, y, W, H; position(&x, &y, &W, &H);
    SetWindowPos(g_hwnd, HWND_TOPMOST, x, y, W, H, 0);
    if (cfg_bool("start_rounded") && cfg_bool("corner_round") && !is_full()) { int r = px(14); SetWindowRgn(g_hwnd, CreateRoundRectRgn(0, 0, W + 1, H + 1, r, r), TRUE); }
    else SetWindowRgn(g_hwnd, NULL, TRUE);
    place_edit();
}

void start_apply(void) {
    if (!g_hwnd) return;
    if (g_edit) SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
    apply_glass(g_hwnd, cfg_bool("glass"), cfg_int("glass_opacity")); reshape(); invalidate();
}

void start_toggle(void) {
    if (g_start_visible) { start_close(); return; }
    if (GetTickCount() - g_closed_at < 250) return;         /* click that just deactivated the menu */
    ensure_window(); scan(); lstrcpyW(g_query, L""); build_rows(); g_tile_scroll = 0;
    apply_glass(g_hwnd, cfg_bool("glass"), cfg_int("glass_opacity")); reshape(); set_edit_text(L"");
    g_start_visible = 1; ShowWindow(g_hwnd, SW_SHOW); SetForegroundWindow(g_hwnd);
    if (g_edit && !collapsed()) SetFocus(g_edit);
    invalidate(); bar_invalidate();
}

void start_destroy(void) { if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; g_edit = NULL; } }
