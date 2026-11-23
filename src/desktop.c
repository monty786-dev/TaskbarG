/* Full desktop mode: custom wallpaper, movable / resizable desktop shortcuts, app tiles and widgets,
 * move lock, and two-way sync with the Wine Desktop folders and the Start panel. */
#include "app.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CLASS_NAME L"WinShellDesktop"
static int g_dirty;
static HWND g_w; static int g_sel = -1, g_hot = -1, g_drag, g_mode, g_sx, g_sy, g_ix, g_iy, g_iw, g_ih, g_tick;

static JNode *items(void) { JNode *a = cfg_node("desktop_items"); return a && a->t == J_ARR ? a : NULL; }
static JNode *item_at(int i) { JNode *a = items(); if (!a) return NULL; for (JNode *c = a->child; c; c = c->next, i--) if (i == 0) return c; return NULL; }
static int gi(JNode *it, const char *k, int d) { JNode *v = it ? json_get(it, k) : NULL; return v && v->t == J_NUM ? (int)v->n : d; }
static const char *gs(JNode *it, const char *k) { JNode *v = it ? json_get(it, k) : NULL; return v && v->t == J_STR && v->s ? v->s : ""; }
static int gb(JNode *it, const char *k) { JNode *v = it ? json_get(it, k) : NULL; return v && v->t == J_BOOL ? v->b : 0; }
static void si(JNode *it, const char *k, int v) { json_put(it, k, json_num(v, 1)); }
static RECT R(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static RECT item_rect(JNode *it) { int x = px(gi(it, "x", 0)), y = px(gi(it, "y", 0)); return R(x, y, x + px(gi(it, "w", 96)), y + px(gi(it, "h", 96))); }
static int cell(void) { int c = cfg_int("grid_size"); return c < 48 ? 48 : c; }
static int locked(JNode *it) { return cfg_bool("desktop_lock") || gb(it, "lock"); }
static int count(void) { JNode *a = items(); return a ? json_count(a) : 0; }

/* ----------------------------------------------------------- item creation */
static int overlaps(int x, int y, int w, int h) {
    JNode *a = items(); for (JNode *c = a ? a->child : NULL; c; c = c->next) { int cx = gi(c, "x", 0), cy = gi(c, "y", 0), cw = gi(c, "w", 96), ch = gi(c, "h", 96);
        if (x < cx + cw && x + w > cx && y < cy + ch && y + h > cy) return 1; }
    return 0;
}
static void free_spot(int w, int h, int *ox, int *oy) {
    int sw, sh; screen_size(&sw, &sh); double sc = cfg_num("scale"); if (sc <= 0) sc = 1; int rows = (int)(sh / sc - 80) / cell(); if (rows < 1) rows = 1; int cols = (int)(sw / sc - 40) / cell();
    for (int cx = 0; cx < cols; cx++) for (int cy = 0; cy < rows; cy++) { int x = 16 + cx * cell(), y = 16 + cy * cell(); if (!overlaps(x, y, w, h)) { *ox = x; *oy = y; return; } }
    *ox = 16; *oy = 16;
}
static JNode *add_item(const char *type, const char *path, const char *widget, int w, int h, const char *src) {
    JNode *a = items(); if (!a) return NULL; JNode *o = json_new(J_OBJ); int x, y; free_spot(w, h, &x, &y);
    json_put(o, "type", json_str(type)); json_put(o, "path", json_str(path)); json_put(o, "widget", json_str(widget)); json_put(o, "src", json_str(src));
    char label[200] = ""; if (path[0]) { wchar_t wp[MAX_PATH], nm[128]; w_from_utf8_buf(path, wp, MAX_PATH); display_name(wp, nm, 128); char *u = utf8_from_w(nm); lstrcpynA(label, u, 200); free(u); }
    json_put(o, "label", json_str(label)); json_put(o, "icon", json_str("")); json_put(o, "color", json_str("")); json_put(o, "lock", json_bool(0));
    si(o, "x", x); si(o, "y", y); si(o, "w", w); si(o, "h", h); json_push(a, o); g_dirty = 1; return o;
}
void desktop_add_shortcut(const wchar_t *path, const char *type) { char *u = utf8_from_w(path); int c = cell(); if (!strcmp(type, "tile")) add_item("tile", u, "", c * 2, c, "user"); else add_item("shortcut", u, "", c, c, "user"); free(u); app_save(); if (g_w) InvalidateRect(g_w, NULL, FALSE); }
void desktop_add_image(const wchar_t *img, const wchar_t *target) { char *u = utf8_from_w(img), *t = utf8_from_w(target ? target : L""); int c = cell(); JNode *o = add_item("image", t, "", c * 2, c * 2, "user"); if (o) json_put(o, "icon", json_str(u)); free(u); free(t); app_save(); if (g_w) InvalidateRect(g_w, NULL, FALSE); }
void desktop_add_widget(const char *kind) { int c = cell(); add_item("widget", "", kind, !strcmp(kind, "calendar") ? c * 2 : c * 2, !strcmp(kind, "calendar") ? c * 2 : c, "user"); app_save(); if (g_w) InvalidateRect(g_w, NULL, FALSE); }
void desktop_clear(void) { JNode *a = items(); if (a) { json_free(a->child); a->child = NULL; } app_save(); if (g_w) InvalidateRect(g_w, NULL, FALSE); }
void desktop_arrange(void) {
    JNode *a = items(); if (!a) return; int sw, sh; screen_size(&sw, &sh); double sc = cfg_num("scale"); if (sc <= 0) sc = 1; int rows = (int)(sh / sc - 80) / cell(); if (rows < 1) rows = 1; int c = cell(), col = 0, row = 0;
    for (JNode *it = a->child; it; it = it->next) { int h = gi(it, "h", c); int spanr = (h + c - 1) / c; if (row + spanr > rows) { row = 0; col++; }
        si(it, "x", 16 + col * c); si(it, "y", 16 + row * c); row += spanr; }
    app_save(); if (g_w) InvalidateRect(g_w, NULL, FALSE);
}

/* -------------------------------------------------- two-way shortcut sync */
static int has_path(const char *key, const char *p) { return cfg_list_has(key, p); }
static void scan_folder(const wchar_t *dir) {
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; _snwprintf(pat, MAX_PATH, L"%ls\\*", dir); HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
    do { if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue; size_t l = wcslen(fd.cFileName);
        if (!(l > 4 && (!_wcsicmp(fd.cFileName + l - 4, L".lnk") || !_wcsicmp(fd.cFileName + l - 4, L".exe") || !_wcsicmp(fd.cFileName + l - 4, L".url")))) continue;
        wchar_t full[MAX_PATH]; _snwprintf(full, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName); char *u = utf8_from_w(full);
        if (cfg_bool("sync_desktop_to_start") && !has_path("sync_seen", u)) {                        /* new Wine / desktop shortcut -> app list + tiles */
            JNode *seen = cfg_node("sync_seen"), *apps = cfg_node("start_apps"), *pins = cfg_node("start_pins");
            if (seen && apps && pins) { g_dirty = 1; json_push(seen, json_str(u)); if (!has_path("start_apps", u)) json_push(apps, json_str(u)); if (!has_path("start_pins", u)) json_push(pins, json_str(u)); }
        }
        if (cfg_bool("full_desktop") && cfg_bool("sync_desktop_to_start") && !has_path("desk_removed", u)) {
            int found = 0; JNode *a = items(); for (JNode *c = a ? a->child : NULL; c && !found; c = c->next) if (!strcmp(gs(c, "path"), u)) found = 1;
            if (!found) add_item("shortcut", u, "", cell(), cell(), "folder");
        }
        free(u);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
void desktop_sync(void) {
    wchar_t d[MAX_PATH], base[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"USERPROFILE", base, MAX_PATH);
    if (n && n < MAX_PATH) { _snwprintf(d, MAX_PATH, L"%ls\\Desktop", base); scan_folder(d); }
    n = GetEnvironmentVariableW(L"PUBLIC", base, MAX_PATH); if (!n || n >= MAX_PATH) lstrcpyW(base, L"C:\\users\\Public"); _snwprintf(d, MAX_PATH, L"%ls\\Desktop", base); scan_folder(d);
    if (cfg_bool("full_desktop") && cfg_bool("sync_start_to_desktop")) {                         /* Start pins / apps -> desktop */
        static const char *keys[] = { "start_pins", "start_apps" };
        for (int k = 0; k < 2; k++) { JNode *a = cfg_node(keys[k]); for (JNode *c = a ? a->child : NULL; c; c = c->next) { if (!c->s || has_path("desk_removed", c->s)) continue;
                int found = 0; JNode *it = items(); for (JNode *x = it ? it->child : NULL; x && !found; x = x->next) if (!strcmp(gs(x, "path"), c->s)) found = 1;
                if (!found) add_item("shortcut", c->s, "", cell(), cell(), "start"); } }
    }
    if (g_dirty) { g_dirty = 0; app_save(); if (g_w) InvalidateRect(g_w, NULL, FALSE); }
}
static void remove_item(int idx) {
    JNode *a = items(); if (!a) return; JNode *c = a->child, *prev = NULL; int i = 0;
    while (c) { JNode *nx = c->next; if (i == idx) { const char *p = gs(c, "path"); JNode *rm = cfg_node("desk_removed"); if (p[0] && rm && !cfg_list_has("desk_removed", p)) json_push(rm, json_str(p));
            if (prev) prev->next = nx; else a->child = nx; c->next = NULL; json_free(c); break; } prev = c; c = nx; i++; }
    g_sel = -1; app_save(); InvalidateRect(g_w, NULL, FALSE);
}

/* ---------------------------------------------------------------- painting */
static void shadow_text(HDC dc, const wchar_t *t, RECT r, UINT f) { RECT s = r; OffsetRect(&s, 1, 1); g_text(dc, t, s, f, RGB(0, 0, 0)); g_text(dc, t, r, f, RGB(255, 255, 255)); }
static COLORREF tile_col(JNode *it, int idx) {
    static const COLORREF P[] = { RGB(0,120,215), RGB(16,137,62), RGB(218,59,1), RGB(142,140,216), RGB(0,153,188), RGB(194,57,179), RGB(209,52,56), RGB(0,178,148) };
    const char *c = gs(it, "color"); return c[0] ? c_hex(c, app_accent()) : P[idx % 8];
}
static int item_alpha(JNode *it) { int a = gi(it, "alpha", -1); if (a < 0) a = cfg_int("widget_alpha"); return a < 0 ? 0 : (a > 100 ? 100 : a); }
static void draw_widget(HDC dc, RECT r, const char *kind, int alpha) {
    Spec sp; app_spec(&sp); COLORREF bg = c_mix(RGB(0, 0, 0), sp.bg, 0.8); if (alpha > 0) g_alpha(dc, r, bg, 255 * alpha / 100); g_frame(dc, r, c_mix(bg, RGB(255, 255, 255), 0.2), 1);
    struct tm t; clock_tm(&t); int w = r.right - r.left, h = r.bottom - r.top; wchar_t buf[96];
    if (!strcmp(kind, "clock")) { wcsftime(buf, 96, L"%I:%M", &t); if (buf[0] == L'0') memmove(buf, buf + 1, wcslen(buf) * sizeof(wchar_t));
        HFONT big = CreateFontW(-(h * 45 / 100), 0, 0, 0, 300, 0, 0, 0, 1, 0, 0, 5, 0, L"Segoe UI"); HGDIOBJ o = SelectObject(dc, big); g_text(dc, buf, R(r.left, r.top + h / 12, r.right, r.top + h * 3 / 4), DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255)); SelectObject(dc, o); DeleteObject(big);
        wcsftime(buf, 96, L"%A, %d %B %Y", &t); HGDIOBJ of = SelectObject(dc, g_font_small); g_text(dc, buf, R(r.left, r.top + h * 3 / 4, r.right, r.bottom - 4), DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(210, 210, 210)); SelectObject(dc, of); }
    else if (!strcmp(kind, "calendar")) { wcsftime(buf, 96, L"%B %Y", &t); HGDIOBJ of = SelectObject(dc, g_font_big); g_text(dc, buf, R(r.left, r.top + 4, r.right, r.top + h / 7 + 4), DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255)); SelectObject(dc, g_font_small);
        static const wchar_t *dn[7] = { L"Su", L"Mo", L"Tu", L"We", L"Th", L"Fr", L"Sa" }; int cw = w / 7, top = r.top + h / 7 + 8, rh = (r.bottom - top - 4) / 7;
        for (int i = 0; i < 7; i++) g_text(dc, dn[i], R(r.left + i * cw, top, r.left + (i + 1) * cw, top + rh), DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(150, 190, 255));
        int first = (t.tm_wday - (t.tm_mday - 1) % 7 + 7) % 7, days = 31; int m = t.tm_mon; if (m == 3 || m == 5 || m == 8 || m == 10) days = 30; else if (m == 1) { int y = t.tm_year + 1900; days = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28; }
        for (int d = 1; d <= days; d++) { int pos = first + d - 1, row = 1 + pos / 7, col = pos % 7; RECT c = R(r.left + col * cw, top + row * rh, r.left + (col + 1) * cw, top + (row + 1) * rh);
            if (d == t.tm_mday) { g_alpha(dc, c, app_accent(), 255); } _snwprintf(buf, 8, L"%d", d); g_text(dc, buf, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255)); }
        SelectObject(dc, of); }
    else { MEMORYSTATUSEX ms; memset(&ms, 0, sizeof ms); ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms); ULONGLONG up = GetTickCount64() / 1000; HGDIOBJ of = SelectObject(dc, g_font);
        _snwprintf(buf, 96, L"Memory  %lu%%", (unsigned long)ms.dwMemoryLoad); g_text(dc, buf, R(r.left + 12, r.top + 8, r.right - 8, r.top + h / 2), DT_VCENTER | DT_SINGLELINE, RGB(255, 255, 255));
        RECT bar = R(r.left + 12, r.top + h / 2 - 2, r.right - 12, r.top + h / 2 + 8); g_fill(dc, bar, RGB(70, 70, 80)); g_fill(dc, R(bar.left, bar.top, bar.left + (bar.right - bar.left) * (int)ms.dwMemoryLoad / 100, bar.bottom), app_accent());
        _snwprintf(buf, 96, L"Uptime  %lud %luh %lum", (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24), (unsigned long)(up / 60 % 60)); g_text(dc, buf, R(r.left + 12, r.top + h / 2 + 10, r.right - 8, r.bottom - 4), DT_VCENTER | DT_SINGLELINE, RGB(210, 210, 210)); SelectObject(dc, of); }
}
static void draw_item(HDC dc, JNode *it, int idx) {
    RECT r = item_rect(it); const char *type = gs(it, "type"); int sel = idx == g_sel, hot = idx == g_hot; int w = r.right - r.left, h = r.bottom - r.top;
    int al = item_alpha(it);
    if (!strcmp(type, "widget")) { draw_widget(dc, r, gs(it, "widget"), al); }
    else if (!strcmp(type, "image")) { wchar_t im[MAX_PATH]; w_from_utf8_buf(gs(it, "icon"), im, MAX_PATH); if (!img_cover(dc, im, r, al)) { g_alpha(dc, r, RGB(60, 60, 70), 255 * al / 100); glyph_draw(G_PIC, dc, R(r.left + w / 4, r.top + h / 4, r.right - w / 4, r.bottom - h / 4), RGB(200, 200, 210)); } }
    else {
        wchar_t path[MAX_PATH], lab[128]; w_from_utf8_buf(gs(it, "path"), path, MAX_PATH); w_from_utf8_buf(gs(it, "label"), lab, 128);
        HICON ic = icon_for_file(path); wchar_t cust[MAX_PATH]; w_from_utf8_buf(gs(it, "icon"), cust, MAX_PATH);
        if (!strcmp(type, "tile")) { if (al > 0) g_alpha(dc, r, tile_col(it, idx), 255 * al / 100); int s = (w < h ? w : h) * 55 / 100; RECT ir = R(r.left + px(10), (r.top + r.bottom) / 2 - s / 2 - px(6), r.left + px(10) + s, (r.top + r.bottom) / 2 + s / 2 - px(6));
            if (!cust[0] || !img_draw(dc, cust, ir, 0, 0)) g_icon_name(dc, ic, lab, ir.left, ir.top, s, 100);
            shadow_text(dc, lab, R(r.left + px(8), r.bottom - px(28), r.right - px(6), r.bottom - px(4)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX); }
        else { if (sel || hot) g_alpha(dc, r, RGB(255, 255, 255), sel ? 70 : 40); int lab_h = cfg_bool("desktop_labels") ? px(34) : 0; int s = (h - lab_h - px(8) < w - px(8) ? h - lab_h - px(8) : w - px(8)); int cap = px(cfg_int("desktop_icon_size")) * 3; if (s > cap) s = cap; if (s < 16) s = 16;
            RECT ir = R((r.left + r.right) / 2 - s / 2, r.top + px(4), (r.left + r.right) / 2 + s / 2, r.top + px(4) + s);
            if (!cust[0] || !img_draw(dc, cust, ir, 0, 0)) g_icon_name(dc, ic, lab, ir.left, ir.top, s, 100);
            if (lab_h) shadow_text(dc, lab, R(r.left + 2, ir.bottom + px(2), r.right - 2, r.bottom - 2), DT_CENTER | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX); }
    }
    if (sel) { g_frame(dc, r, app_accent(), 2); if (!locked(it)) g_fill(dc, R(r.right - px(10), r.bottom - px(10), r.right, r.bottom), app_accent()); }
    if (locked(it) && (sel || hot)) glyph_draw(G_PIN, dc, R(r.right - px(18), r.top + 2, r.right - 2, r.top + px(18)), RGB(255, 255, 255));
}
static void paint_all(HDC dc, int W, int H) {
    COLORREF bg = c_hex(cfg_str("desktop_color"), RGB(30, 58, 95)); g_fill(dc, R(0, 0, W, H), bg);
    wchar_t wp[MAX_PATH]; w_from_utf8_buf(cfg_str("wallpaper"), wp, MAX_PATH); if (wp[0]) img_fill(dc, wp, W, H, cfg_str("wallpaper_mode"));
    SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font); JNode *a = items(); int i = 0;
    for (JNode *c = a ? a->child : NULL; c; c = c->next, i++) draw_item(dc, c, i);
    SelectObject(dc, of);
}
static int hit(int x, int y, int *grip) {
    *grip = 0; int n = count();
    for (int i = n - 1; i >= 0; i--) { JNode *it = item_at(i); RECT r = item_rect(it); if (!in_rect(&r, x, y)) continue; *grip = (i == g_sel && !locked(it) && x >= r.right - px(14) && y >= r.bottom - px(14)); return i; }
    return -1;
}

/* ------------------------------------------------------------- interaction */
static int pick(const char *kind, char *out, int n) {
    static const wchar_t IMG[] = L"Images\0*.png;*.ico;*.jpg;*.jpeg;*.bmp\0All files\0*.*\0"; static const wchar_t EXE[] = L"Programs\0*.exe;*.lnk;*.bat\0All files\0*.*\0";
    wchar_t f[MAX_PATH] = L""; OPENFILENAMEW o; memset(&o, 0, sizeof o); o.lStructSize = sizeof o; o.hwndOwner = g_w; o.lpstrFile = f; o.nMaxFile = MAX_PATH; o.lpstrFilter = !strcmp(kind, "img") ? IMG : EXE; o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_menu_depth++; BOOL ok = GetOpenFileNameW(&o); g_menu_depth--; if (!ok) return 0; char *u = utf8_from_w(f); lstrcpynA(out, u, n); free(u); return 1;
}
static void snap(JNode *it) { if (!cfg_bool("desktop_grid")) return; int c = cell(); si(it, "x", 16 + ((gi(it, "x", 0) - 16 + c / 2) / c) * c); si(it, "y", 16 + ((gi(it, "y", 0) - 16 + c / 2) / c) * c); }
static void set_size(JNode *it, int wc, int hc) { int c = cell(); si(it, "w", wc * c / 2); si(it, "h", hc * c / 2); }

static void item_menu(int idx) {
    JNode *it = item_at(idx); if (!it) return; int lk = gb(it, "lock"); const char *type = gs(it, "type");
    const wchar_t *lab[20]; int id[20], fl[20], n = 0;
    #define ADD(i, t, f) do { lab[n] = (t); id[n] = (i); fl[n] = (f); n++; } while (0)
    if (strcmp(type, "widget")) ADD(1, L"Open", 0); ADD(2, lk ? L"Unlock this item" : L"Lock this item", 0); ADD(-1, NULL, 0);
    ADD(3, L"Size: small", 0); ADD(4, L"Size: medium", 0); ADD(5, L"Size: wide", 0); ADD(6, L"Size: large", 0);
    if (!strcmp(type, "widget") || !strcmp(type, "tile") || !strcmp(type, "image")) { ADD(-1, NULL, 0); ADD(11, L"More transparent (-10%)", 0); ADD(12, L"Less transparent (+10%)", 0); ADD(13, L"Fully transparent (0%)", 0); ADD(14, L"Fully visible (100%)", 0); }
    if (strcmp(type, "widget")) { ADD(7, !strcmp(type, "image") ? L"Change picture..." : L"Change icon...", 0); ADD(8, L"Pin to Start", 0); ADD(9, L"Add to tray", 0); } ADD(-1, NULL, 0); ADD(10, L"Remove from desktop", 0);
    #undef ADD
    int cmd = app_popup(g_w, lab, id, fl, n, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR); wchar_t wp[MAX_PATH]; w_from_utf8_buf(gs(it, "path"), wp, MAX_PATH); char buf[320];
    switch (cmd) {
    case 1: app_run(wp, NULL, SW_SHOWNORMAL); break; case 2: json_put(it, "lock", json_bool(!lk)); break;
    case 3: set_size(it, 1, 1); break; case 4: set_size(it, 2, 2); break; case 5: set_size(it, 4, 2); break; case 6: set_size(it, 4, 4); break;
    case 7: if (pick("img", buf, 320)) json_put(it, "icon", json_str(buf)); break;
    case 8: { JNode *p = cfg_node("start_pins"); if (p && !cfg_list_has("start_pins", gs(it, "path"))) json_push(p, json_str(gs(it, "path"))); break; }
    case 9: tray_add_app(wp); break; case 10: remove_item(idx); return;
    case 11: case 12: { int a = item_alpha(it) + (cmd == 11 ? -10 : 10); si(it, "alpha", a < 0 ? 0 : (a > 100 ? 100 : a)); break; }
    case 13: si(it, "alpha", 0); break; case 14: si(it, "alpha", 100); break;
    }
    if (cmd > 0) { app_save(); InvalidateRect(g_w, NULL, FALSE); }
}
static void desk_menu(void) {
    const wchar_t *lab[] = { L"Add shortcut...", L"Add app tile...", L"Add clock widget", L"Add calendar widget", L"Add system widget", L"Add image tile...", NULL, L"Lock all icons (no moving)", L"Auto-arrange", L"Sync Wine / desktop shortcuts", NULL, L"Change wallpaper...", L"Switch to normal mode", L"Desktop settings" };
    int id[] = { 1, 2, 3, 4, 5, 11, -1, 6, 7, 8, -1, 9, 12, 10 }, fl[] = { 0, 0, 0, 0, 0, 0, 0, cfg_bool("desktop_lock") ? MF_CHECKED : 0, 0, 0, 0, 0, 0, 0 };
    int cmd = app_popup(g_w, lab, id, fl, 14, TPM_RETURNCMD | TPM_RIGHTBUTTON, POPUP_CUR, POPUP_CUR); char buf[320];
    if (cmd == 1 || cmd == 2) { if (pick("exe", buf, 320)) { wchar_t w[MAX_PATH]; w_from_utf8_buf(buf, w, MAX_PATH); desktop_add_shortcut(w, cmd == 1 ? "shortcut" : "tile"); } }
    else if (cmd == 3) desktop_add_widget("clock"); else if (cmd == 4) desktop_add_widget("calendar"); else if (cmd == 5) desktop_add_widget("system");
    else if (cmd == 6) { cfg_set_bool("desktop_lock", !cfg_bool("desktop_lock")); app_save(); } else if (cmd == 7) desktop_arrange(); else if (cmd == 8) desktop_sync();
    else if (cmd == 9) { if (pick("img", buf, 320)) { cfg_set_str("wallpaper", buf); app_save(); img_clear(); } } else if (cmd == 10) settings_open_page("Full desktop"); else if (cmd == 12) { app_toggle_desktop(); return; }
    else if (cmd == 11) { if (pick("img", buf, 320)) { wchar_t w[MAX_PATH]; w_from_utf8_buf(buf, w, MAX_PATH); desktop_add_image(w, NULL); } }
    InvalidateRect(g_w, NULL, FALSE);
}

static void inv(int idx) { JNode *it = item_at(idx); if (!it || !g_w) return; RECT r = item_rect(it); InflateRect(&r, px(6), px(6)); InvalidateRect(g_w, &r, FALSE); }

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int hd; LRESULT mr;
    switch (m) {
    case WM_PAINT: { PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc); HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        paint_all(mem, rc.right, rc.bottom); BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_MEASUREITEM: case WM_DRAWITEM: mr = app_menu_msg(m, w, l, &hd); if (hd) return mr; break;
    case WM_TIMER: g_tick++; { JNode *a = items(); for (JNode *c = a ? a->child : NULL; c; c = c->next) if (!strcmp(gs(c, "type"), "widget")) { InvalidateRect(h, NULL, FALSE); break; } } return 0;
    case WM_LBUTTONDOWN: { int x = (short)LOWORD(l), y = (short)HIWORD(l), grip; int i = hit(x, y, &grip); if (g_sel != i) { inv(g_sel); g_sel = i; } if (i < 0) { InvalidateRect(h, NULL, FALSE); return 0; }
        JNode *it = item_at(i); inv(i); if (locked(it)) return 0; g_drag = 1; g_mode = grip ? 2 : 1; g_sx = x; g_sy = y; g_ix = gi(it, "x", 0); g_iy = gi(it, "y", 0); g_iw = gi(it, "w", 96); g_ih = gi(it, "h", 96); SetCapture(h); return 0; }
    case WM_MOUSEMOVE: { int x = (short)LOWORD(l), y = (short)HIWORD(l);
        if (g_drag && g_sel >= 0) { JNode *it = item_at(g_sel); double sc = cfg_num("scale"); if (sc <= 0) sc = 1; int dx = (int)((x - g_sx) / sc), dy = (int)((y - g_sy) / sc); inv(g_sel);
            if (g_mode == 1) { si(it, "x", g_ix + dx); si(it, "y", g_iy + dy); } else { si(it, "w", g_iw + dx < 40 ? 40 : g_iw + dx); si(it, "h", g_ih + dy < 40 ? 40 : g_ih + dy); } inv(g_sel); return 0; }
        int g2, i = hit(x, y, &g2); if (i != g_hot) { inv(g_hot); g_hot = i; inv(i); } return 0; }
    case WM_LBUTTONUP: if (g_drag) { g_drag = 0; ReleaseCapture(); if (g_sel >= 0) { JNode *it = item_at(g_sel); if (it) { if (g_mode == 1) snap(it); else { int c = cell(); si(it, "w", (gi(it, "w", c) + c / 4) / (c / 2) * (c / 2)); si(it, "h", (gi(it, "h", c) + c / 4) / (c / 2) * (c / 2)); } } } app_save(); InvalidateRect(h, NULL, FALSE); } return 0;
    case WM_LBUTTONDBLCLK: { int x = (short)LOWORD(l), y = (short)HIWORD(l), g; int i = hit(x, y, &g); if (i >= 0) { JNode *it = item_at(i); if (strcmp(gs(it, "type"), "widget") && gs(it, "path")[0]) { wchar_t wp[MAX_PATH]; w_from_utf8_buf(gs(it, "path"), wp, MAX_PATH); app_run(wp, NULL, SW_SHOWNORMAL); } } return 0; }
    case WM_RBUTTONUP: { int x = (short)LOWORD(l), y = (short)HIWORD(l), g; int i = hit(x, y, &g); if (i >= 0) { inv(g_sel); g_sel = i; inv(i); item_menu(i); } else desk_menu(); return 0; }
    }
    return DefWindowProcW(h, m, w, l);
}

void desktop_apply(void) {
    if (!cfg_bool("full_desktop")) { if (g_w) ShowWindow(g_w, SW_HIDE); return; }
    if (!g_w) {
        WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.style = CS_DBLCLKS; wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
        int sw, sh; screen_size(&sw, &sh); g_w = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, CLASS_NAME, L"WinShell Desktop", WS_POPUP, 0, 0, sw, sh, NULL, NULL, wc.hInstance, NULL); if (!g_w) return; SetTimer(g_w, 1, 1000, NULL);
    }
    int sw, sh; screen_size(&sw, &sh); SetWindowPos(g_w, HWND_BOTTOM, 0, 0, sw, sh, SWP_NOACTIVATE); ShowWindow(g_w, SW_SHOWNA); desktop_sync(); InvalidateRect(g_w, NULL, FALSE);
}
void desktop_tick(void) {
    g_tick++; if (g_tick % 250 == 0) desktop_sync();            /* ~10 s: pick up new Wine / desktop shortcuts */
    if (g_w && IsWindowVisible(g_w) && g_tick % 25 == 0 && !g_drag) SetWindowPos(g_w, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
void desktop_destroy(void) { if (g_w) { DestroyWindow(g_w); g_w = NULL; } }
