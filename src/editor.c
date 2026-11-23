/* Layout editor: a special window with a live preview of the taskbar or Start panel and a
 * properties panel. Move and resize elements freely, hide (delete) or show (add) them,
 * change icon and colour, switch auto position / auto size, add programs. */
#include "app.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLASS_NAME L"WinShellEditor"
enum { C_NONE, C_TAB, C_CHIP, C_VIS, C_AUTOPOS, C_AUTOSIZE, C_SLX, C_SLY, C_SLW, C_SLH, C_ICON, C_ICON_CLR, C_COL, C_COL_CLR, C_RESET, C_DELETE, C_ADD_BAR, C_ADD_TILE, C_AUTO_ALL, C_ZOOM_IN, C_ZOOM_OUT, C_CLOSE, C_MIN };
typedef struct { RECT r; int id, a; int lo, hi, val; } Ctl;
static Ctl g_c[80]; static int g_nc;
static HWND g_w; static int g_tab, g_sel = -1, g_pscroll, g_sl = -1; static double g_zoom = 1.0, g_s = 1.0; static int g_panx, g_pany, g_W, g_H;
static int g_ids[32]; static RECT g_rc[32]; static int g_nel;
static int g_drag, g_mode, g_sx, g_sy; static Place g_p0; static RECT g_r0; static DWORD g_live;

static RECT RC(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int cap_h(void) { return px(34); }
static int panel_w(void) { RECT rc; GetClientRect(g_w, &rc); int w = rc.right * 36 / 100; return w < px(330) ? px(330) : w; }
static RECT canvas(void) { RECT rc; GetClientRect(g_w, &rc); return RC(px(16), cap_h() + px(56), rc.right - panel_w() - px(10), rc.bottom - px(16)); }
static int unpx(int v) { double sc = cfg_num("scale"); if (sc <= 0) sc = 1.0; return (int)(v / sc); }
static void tab_elements(int *out, int *n) {
    static const int bar[] = { E_START, E_SEARCH, E_DESK, E_PREVIEW, E_CLOCK, E_TRAY, E_MODE, E_APPS }; *n = 0;
    if (g_tab == 0) for (int i = 0; i < 8; i++) out[(*n)++] = bar[i]; else for (int e = E_SM_USER; e <= E_SM_MODE; e++) out[(*n)++] = e;
}
static void refresh(void) { g_nel = g_tab == 0 ? bar_element_rects(g_ids, g_rc, 32, &g_W, &g_H) : start_element_rects(g_ids, g_rc, 32, &g_W, &g_H); if (g_W < 1) g_W = 1; if (g_H < 1) g_H = 1; }
static RECT rect_of(int e) { for (int i = 0; i < g_nel; i++) if (g_ids[i] == e) return g_rc[i]; return RC(0, 0, 0, 0); }
static void commit(int live) {
    if (live && GetTickCount() - g_live < 70) return; g_live = GetTickCount(); app_save(); app_apply_all(); refresh(); if (g_w) InvalidateRect(g_w, NULL, FALSE);
}
static void view(RECT cv) {
    int cw = cv.right - cv.left, ch = cv.bottom - cv.top; double fit = (double)cw / g_W < (double)ch / g_H ? (double)cw / g_W : (double)ch / g_H; g_s = fit * g_zoom;
    if (g_tab == 0) { double t = bar_vertical() ? g_W : g_H; if (t * g_s < px(60)) g_s = px(60) / t; }
    int mx = (int)(g_W * g_s) - cw, my = (int)(g_H * g_s) - ch; if (mx < 0) mx = 0; if (my < 0) my = 0; if (g_panx > mx) g_panx = mx; if (g_pany > my) g_pany = my; if (g_panx < 0) g_panx = 0; if (g_pany < 0) g_pany = 0;
}
static RECT to_canvas(RECT r, RECT cv) { return RC(cv.left - g_panx + (int)(r.left * g_s), cv.top - g_pany + (int)(r.top * g_s), cv.left - g_panx + (int)(r.right * g_s), cv.top - g_pany + (int)(r.bottom * g_s)); }

/* ----------------------------------------------------------------- panel */
typedef struct { COLORREF bg, panel, text, dim, accent, ctl, border; } Pal;
static Pal pal(void) { Spec sp; app_spec(&sp); Pal p; p.bg = sp.bg; p.panel = sp.alt; p.text = sp.text; p.dim = c_mix(sp.text, sp.bg, 0.45); p.accent = app_accent(); p.ctl = c_mix(sp.bg, sp.text, 0.12); p.border = c_mix(sp.bg, sp.text, 0.32); return p; }
static Ctl *add(int id, int a, RECT r) { if (g_nc >= 80) return &g_c[79]; Ctl *c = &g_c[g_nc++]; memset(c, 0, sizeof *c); c->id = id; c->a = a; c->r = r; return c; }
static void button(HDC dc, const Pal *p, int id, const wchar_t *t, RECT r, int on) {
    add(id, 0, r); if (cfg_bool("ui_pills")) { g_pill(dc, r, t, 1, on); return; }
    g_fill(dc, r, on ? p->accent : p->ctl); g_frame(dc, r, on ? p->accent : p->border, 1); g_text(dc, t, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, on ? RGB(255, 255, 255) : p->text);
}
static void toggle(HDC dc, const Pal *p, int id, const wchar_t *label, int on, int x, int y, int w) {
    RECT sw = RC(x + w - px(46), y + px(4), x + w, y + px(24)); add(id, 0, RC(x, y, x + w, y + px(28)));
    g_text(dc, label, RC(x, y, sw.left - px(8), y + px(28)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text);
    if (cfg_bool("ui_pills")) { g_pill(dc, sw, on ? L"ON" : L"OFF", 2, on); return; }
    HBRUSH b = CreateSolidBrush(on ? p->accent : p->bg); HPEN pen = CreatePen(PS_SOLID, 2, on ? p->accent : p->dim); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen); int h = sw.bottom - sw.top;
    RoundRect(dc, sw.left, sw.top, sw.right, sw.bottom, h, h); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(pen);
    int d = h - px(6); int kx = on ? sw.right - d - px(3) : sw.left + px(3); HBRUSH k = CreateSolidBrush(on ? RGB(255, 255, 255) : p->dim); ob = SelectObject(dc, k); op = SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, kx, sw.top + px(3), kx + d, sw.top + px(3) + d); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(k);
}
static void slider(HDC dc, const Pal *p, int id, const wchar_t *label, int val, int lo, int hi, int x, int y, int w) {
    wchar_t t[64]; _snwprintf(t, 64, L"%ls: %d", label, val); g_text(dc, t, RC(x, y, x + w, y + px(20)), DT_VCENTER | DT_SINGLELINE, p->text);
    RECT tr = RC(x, y + px(22), x + w, y + px(38)); Ctl *c = add(id, 0, tr); c->lo = lo; c->hi = hi; c->val = val; int cy = (tr.top + tr.bottom) / 2; double f = hi > lo ? (double)(val - lo) / (hi - lo) : 0; if (f < 0) f = 0; if (f > 1) f = 1;
    g_fill(dc, RC(tr.left, cy - 2, tr.right, cy + 2), p->border); int kx = tr.left + (int)((tr.right - tr.left) * f); g_fill(dc, RC(tr.left, cy - 2, kx, cy + 2), p->accent);
    HBRUSH b = CreateSolidBrush(p->accent); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, kx - px(7), cy - px(7), kx + px(7), cy + px(7)); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
}
static void panel(HDC dc, const Pal *p, int W, int H) {
    int px0 = W - panel_w(), x = px0 + px(14), w = panel_w() - px(28), y = cap_h() + px(8) - g_pscroll; g_fill(dc, RC(px0, cap_h(), W, H), p->panel);
    HRGN rgn = CreateRectRgn(px0, cap_h(), W, H); SelectClipRgn(dc, rgn);
    HGDIOBJ of = SelectObject(dc, g_font_big); g_text(dc, g_tab == 0 ? L"Taskbar elements" : L"Start panel elements", RC(x, y, x + w, y + px(28)), DT_VCENTER | DT_SINGLELINE, p->text); SelectObject(dc, of); y += px(32);
    int list[32], n; tab_elements(list, &n); int cx = x, rowh = px(30);                                     /* chips: click to select; dimmed = hidden */
    for (int i = 0; i < n; i++) { wchar_t nm[60]; w_from_utf8_buf(el_label(list[i]), nm, 60); const wchar_t *s = wcsncmp(nm, L"Start: ", 7) == 0 ? nm + 7 : nm; SIZE sz = { 0, 0 }; GetTextExtentPoint32W(dc, s, lstrlenW(s), &sz); int cw = sz.cx + px(22);
        if (cx + cw > x + w) { cx = x; y += rowh; } RECT r = RC(cx, y, cx + cw, y + px(26)); add(C_CHIP, list[i], r); int on = list[i] == g_sel, vis = el_show(list[i]);
        if (cfg_bool("ui_pills")) g_pill(dc, r, s, 1, on); else { g_fill(dc, r, on ? p->accent : p->ctl); g_frame(dc, r, vis ? p->accent : p->border, 1); g_text(dc, s, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, on ? RGB(255, 255, 255) : (vis ? p->text : p->dim)); }
        cx += cw + px(6); }
    y += rowh + px(10);
    if (g_sel >= 0) {
        Place pl; el_get_place(g_sel, &pl); RECT cur = rect_of(g_sel); int vis = el_show(g_sel); wchar_t nm[80]; w_from_utf8_buf(el_label(g_sel), nm, 80);
        HGDIOBJ o2 = SelectObject(dc, g_font_big); g_text(dc, nm, RC(x, y, x + w, y + px(28)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->accent); SelectObject(dc, o2); y += px(34);
        toggle(dc, p, C_VIS, L"Visible (off = delete, on = add)", vis, x, y, w); y += px(32); toggle(dc, p, C_AUTOPOS, L"Auto position", pl.aut, x, y, w); y += px(32); toggle(dc, p, C_AUTOSIZE, L"Auto size", pl.autosize, x, y, w); y += px(36);
        int rw_ = cur.right - cur.left, rh_ = cur.bottom - cur.top;
        slider(dc, p, C_SLX, pl.aut ? L"Move left / right (offset)" : L"X position", pl.aut ? pl.dx : pl.ax, pl.aut ? -600 : 0, pl.aut ? 600 : 2000, x, y, w); y += px(46);
        slider(dc, p, C_SLY, pl.aut ? L"Move up / down (offset)" : L"Y position", pl.aut ? pl.dy : pl.ay, pl.aut ? -400 : 0, pl.aut ? 400 : 1400, x, y, w); y += px(46);
        slider(dc, p, C_SLW, L"Width", pl.autosize ? unpx(rw_) : pl.w, 8, 800, x, y, w); y += px(46); slider(dc, p, C_SLH, L"Height", pl.autosize ? unpx(rh_) : pl.h, 8, 800, x, y, w); y += px(50);
        int bw = (w - px(8)) / 2; button(dc, p, C_ICON, L"Icon image...", RC(x, y, x + bw, y + px(32)), 0); button(dc, p, C_ICON_CLR, L"Clear icon", RC(x + bw + px(8), y, x + w, y + px(32)), 0); y += px(38);
        button(dc, p, C_COL, L"Colour...", RC(x, y, x + bw, y + px(32)), 0); button(dc, p, C_COL_CLR, L"Clear colour", RC(x + bw + px(8), y, x + w, y + px(32)), 0); y += px(38);
        button(dc, p, C_RESET, L"Reset this", RC(x, y, x + bw, y + px(32)), 0); button(dc, p, C_DELETE, L"Delete (hide)", RC(x + bw + px(8), y, x + w, y + px(32)), 0); y += px(44);
    } else { g_text(dc, L"Click an element in the preview or a name above.", RC(x, y, x + w, y + px(40)), DT_WORDBREAK, p->dim); y += px(50); }
    button(dc, p, C_ADD_BAR, L"Add a program to the taskbar...", RC(x, y, x + w, y + px(32)), 0); y += px(38); button(dc, p, C_ADD_TILE, L"Add a program to Start tiles...", RC(x, y, x + w, y + px(32)), 0); y += px(38);
    button(dc, p, C_AUTO_ALL, L"Auto-adapt everything (reset all)", RC(x, y, x + w, y + px(32)), 0); y += px(40);
    SelectClipRgn(dc, NULL); DeleteObject(rgn);
}

static void paint(HDC dc, int W, int H) {
    Pal p = pal(); g_nc = 0; g_fill(dc, RC(0, 0, W, H), p.bg); SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font); refresh(); RECT cv = canvas(); view(cv);
    /* canvas: live preview */
    g_fill(dc, cv, c_mix(p.bg, RGB(0, 0, 0), 0.35)); HRGN rgn = CreateRectRgn(cv.left, cv.top, cv.right, cv.bottom); SelectClipRgn(dc, rgn);
    { HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, g_W, g_H); HGDIOBJ old = SelectObject(mem, bmp); if (g_tab == 0) bar_render(mem, g_W, g_H); else start_render(mem, g_W, g_H);
      SetStretchBltMode(dc, HALFTONE); StretchBlt(dc, cv.left - g_panx, cv.top - g_pany, (int)(g_W * g_s), (int)(g_H * g_s), mem, 0, 0, g_W, g_H, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); }
    for (int i = 0; i < g_nel; i++) { RECT r = to_canvas(g_rc[i], cv); int sel = g_ids[i] == g_sel; g_alpha(dc, r, p.accent, sel ? 60 : 25); g_frame(dc, r, p.accent, sel ? 2 : 1);
        if (sel) { RECT gr = RC(r.right - px(10), r.bottom - px(10), r.right, r.bottom); g_fill(dc, gr, p.accent); } }
    SelectClipRgn(dc, NULL); DeleteObject(rgn); g_frame(dc, cv, p.border, 1);
    /* tabs + zoom */
    button(dc, &p, C_TAB, L"Taskbar", RC(px(16), cap_h() + px(12), px(16) + px(120), cap_h() + px(44)), g_tab == 0); g_c[g_nc - 1].a = 0;
    button(dc, &p, C_TAB, L"Start panel", RC(px(16) + px(128), cap_h() + px(12), px(16) + px(256), cap_h() + px(44)), g_tab == 1); g_c[g_nc - 1].a = 1;
    button(dc, &p, C_ZOOM_OUT, L"-", RC(cv.right - px(80), cap_h() + px(12), cv.right - px(44), cap_h() + px(44)), 0); button(dc, &p, C_ZOOM_IN, L"+", RC(cv.right - px(40), cap_h() + px(12), cv.right, cap_h() + px(44)), 0);
    g_text(dc, L"Drag to move, drag the corner square to resize. Mouse wheel pans a zoomed preview.", RC(px(280), cap_h() + px(12), cv.right - px(90), cap_h() + px(44)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, p.dim);
    panel(dc, &p, W, H);
    /* caption */
    g_fill(dc, RC(0, 0, W, cap_h()), c_mix(p.bg, p.text, 0.05)); g_text(dc, L"WinShell layout editor", RC(px(14), 0, W - px(100), cap_h()), DT_VCENTER | DT_SINGLELINE, p.text);
    RECT cl = RC(W - px(46), 0, W, cap_h()), mn = RC(W - px(92), 0, W - px(46), cap_h()); add(C_CLOSE, 0, cl); add(C_MIN, 0, mn);
    glyph_draw(G_MIN, dc, RC(mn.left + px(14), mn.top + px(8), mn.right - px(14), mn.bottom - px(8)), p.text); glyph_draw(G_CLOSE, dc, RC(cl.left + px(14), cl.top + px(8), cl.right - px(14), cl.bottom - px(8)), p.text);
    g_frame(dc, RC(0, 0, W, H), p.border, 1); SelectObject(dc, of);
}

/* ------------------------------------------------------------ interaction */
static Ctl *ctl_at(int x, int y) { for (int i = g_nc - 1; i >= 0; i--) if (in_rect(&g_c[i].r, x, y)) { return &g_c[i]; } return NULL; }

static int pick_file(int img, char *out, int n) {
    static const wchar_t IMG[] = L"Images\0*.png;*.ico;*.jpg;*.jpeg;*.bmp;*.gif\0All files\0*.*\0"; static const wchar_t EXE[] = L"Programs\0*.exe;*.lnk;*.bat\0All files\0*.*\0";
    wchar_t f[MAX_PATH] = L""; OPENFILENAMEW o; memset(&o, 0, sizeof o); o.lStructSize = sizeof o; o.hwndOwner = g_w; o.lpstrFile = f; o.nMaxFile = MAX_PATH; o.lpstrFilter = img ? IMG : EXE; o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_menu_depth++; BOOL ok = GetOpenFileNameW(&o); g_menu_depth--; if (!ok) return 0; char *u = utf8_from_w(f); lstrcpynA(out, u, n); free(u); return 1;
}
static void logical(RECT r, int *u, int *v, int *lu, int *lv) {
    int vert = g_tab == 0 && bar_vertical(); *u = vert ? r.top : r.left; *v = vert ? r.left : r.top; *lu = vert ? r.bottom - r.top : r.right - r.left; *lv = vert ? r.right - r.left : r.bottom - r.top;
}
static void apply_slider(int id, int v) {
    if (g_sel < 0) return; Place p; el_get_place(g_sel, &p); RECT cur = rect_of(g_sel); int u, vv, lu, lv; logical(cur, &u, &vv, &lu, &lv);
    if (id == C_SLX) { if (p.aut) p.dx = v; else p.ax = v; } else if (id == C_SLY) { if (p.aut) p.dy = v; else p.ay = v; }
    else { if (p.autosize) { p.autosize = 0; p.w = unpx(lu); p.h = unpx(lv); } if (id == C_SLW) p.w = v; else p.h = v; }
    el_set_place(g_sel, &p); commit(1);
}
static void slider_drag(int x) {
    for (int i = 0; i < g_nc; i++) if (g_c[i].id == g_sl) { Ctl *c = &g_c[i]; double f = (double)(x - c->r.left) / (c->r.right - c->r.left); if (f < 0) f = 0; if (f > 1) f = 1; apply_slider(g_sl, c->lo + (int)(f * (c->hi - c->lo) + 0.5)); return; }
}
static void do_ctl(Ctl *c, int x) {
    char b[320]; Place p; if (g_sel >= 0) el_get_place(g_sel, &p); else memset(&p, 0, sizeof p); RECT cur = g_sel >= 0 ? rect_of(g_sel) : RC(0, 0, 0, 0); int u, v, lu, lv; logical(cur, &u, &v, &lu, &lv);
    switch (c->id) {
    case C_TAB: g_tab = c->a; g_sel = -1; g_panx = g_pany = 0; g_pscroll = 0; break;
    case C_CHIP: g_sel = c->a; break;
    case C_VIS: el_set_show(g_sel, !el_show(g_sel)); break;
    case C_AUTOPOS: if (p.aut) { p.aut = 0; p.ax = unpx(u); p.ay = unpx(v); } else p.aut = 1; el_set_place(g_sel, &p); break;
    case C_AUTOSIZE: if (p.autosize) { p.autosize = 0; p.w = unpx(lu); p.h = unpx(lv); } else p.autosize = 1; el_set_place(g_sel, &p); break;
    case C_SLX: case C_SLY: case C_SLW: case C_SLH: g_sl = c->id; slider_drag(x); return;
    case C_ICON: if (pick_file(1, b, 320)) el_set_icon(g_sel, b); else return; break;
    case C_ICON_CLR: el_set_icon(g_sel, ""); break;
    case C_COL: { static COLORREF cust[16]; CHOOSECOLORW cc; memset(&cc, 0, sizeof cc); cc.lStructSize = sizeof cc; cc.hwndOwner = g_w; cc.rgbResult = el_color(g_sel, app_accent()); cc.lpCustColors = cust; cc.Flags = CC_RGBINIT | CC_FULLOPEN;
        g_menu_depth++; BOOL ok = ChooseColorW(&cc); g_menu_depth--; if (!ok) return; char hx[8]; snprintf(hx, 8, "#%02X%02X%02X", GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult)); el_set_color(g_sel, hx); break; }
    case C_COL_CLR: el_set_color(g_sel, ""); break;
    case C_RESET: memset(&p, 0, sizeof p); p.aut = 1; p.autosize = 1; el_set_place(g_sel, &p); el_set_show(g_sel, 1); break;
    case C_DELETE: el_set_show(g_sel, 0); break;
    case C_ADD_BAR: if (pick_file(0, b, 320)) { wchar_t w[MAX_PATH]; w_from_utf8_buf(b, w, MAX_PATH); bar_pin(w); } else return; break;
    case C_ADD_TILE: if (pick_file(0, b, 320)) { JNode *a = cfg_node("start_pins"), *s = cfg_node("start_apps"); if (a && !cfg_list_has("start_pins", b)) json_push(a, json_str(b)); if (s && !cfg_list_has("start_apps", b)) json_push(s, json_str(b)); } else return; break;
    case C_AUTO_ALL: el_auto_all(); break;
    case C_ZOOM_IN: g_zoom *= 1.25; if (g_zoom > 8) g_zoom = 8; InvalidateRect(g_w, NULL, FALSE); return;
    case C_ZOOM_OUT: g_zoom /= 1.25; if (g_zoom < 1) g_zoom = 1; InvalidateRect(g_w, NULL, FALSE); return;
    case C_CLOSE: ShowWindow(g_w, SW_HIDE); return; case C_MIN: ShowWindow(g_w, SW_MINIMIZE); return;
    default: return;
    }
    commit(0);
}
static int hit_element(int x, int y, RECT cv) {
    int best = -1; long long area = 0; for (int i = 0; i < g_nel; i++) { RECT r = to_canvas(g_rc[i], cv); if (!in_rect(&r, x, y)) continue; long long a = (long long)(r.right - r.left) * (r.bottom - r.top); if (best < 0 || a < area) { best = i; area = a; } }
    return best;
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: { PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc); HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
        paint(mem, rc.right, rc.bottom); BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_NCCALCSIZE: if (w) return 0; break;
    case WM_GETMINMAXINFO: { MINMAXINFO *mi = (MINMAXINFO *)l; mi->ptMinTrackSize.x = px(820); mi->ptMinTrackSize.y = px(560); return 0; }
    case WM_NCHITTEST: { POINT pt = { (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &pt); RECT rc; GetClientRect(h, &rc); int b = px(6), W = rc.right, H = rc.bottom;
        int lf = pt.x < b, rt = pt.x >= W - b, tp = pt.y < b, bt = pt.y >= H - b; if (tp && lf) return HTTOPLEFT; if (tp && rt) return HTTOPRIGHT; if (bt && lf) return HTBOTTOMLEFT; if (bt && rt) return HTBOTTOMRIGHT;
        if (lf) return HTLEFT; if (rt) return HTRIGHT; if (tp) return HTTOP; if (bt) return HTBOTTOM; if (pt.y < cap_h() && pt.x < W - px(92)) return HTCAPTION; return HTCLIENT; }
    case WM_SIZE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: { int x = (short)LOWORD(l), y = (short)HIWORD(l); Ctl *c = NULL; for (int i = g_nc - 1; i >= 0; i--) if (in_rect(&g_c[i].r, x, y)) { c = &g_c[i]; break; }
        RECT cv = canvas();
        if (c && !(c->id == C_NONE)) { Ctl cc = *c; do_ctl(&cc, x); InvalidateRect(h, NULL, FALSE); if (g_sl >= 0) SetCapture(h); return 0; }
        if (in_rect(&cv, x, y)) {
            int grip = -1; if (g_sel >= 0) { RECT r = to_canvas(rect_of(g_sel), cv); if (x >= r.right - px(12) && x <= r.right + px(2) && y >= r.bottom - px(12) && y <= r.bottom + px(2)) grip = 1; }
            int i = grip >= 0 ? -2 : hit_element(x, y, cv); if (i >= 0) g_sel = g_ids[i];
            if (g_sel >= 0 && (i >= 0 || grip >= 0)) { g_drag = 1; g_mode = grip >= 0 ? 2 : 1; g_sx = x; g_sy = y; el_get_place(g_sel, &g_p0); g_r0 = rect_of(g_sel); SetCapture(h); }
            InvalidateRect(h, NULL, FALSE);
        }
        return 0; }
    case WM_MOUSEMOVE: { int x = (short)LOWORD(l), y = (short)HIWORD(l);
        if (g_sl >= 0) { slider_drag(x); return 0; }
        if (g_drag && g_sel >= 0) { int vert = g_tab == 0 && bar_vertical(); double dxs = (x - g_sx) / g_s, dys = (y - g_sy) / g_s; int du = (int)(vert ? dys : dxs), dv = (int)(vert ? dxs : dys); Place p = g_p0;
            if (g_mode == 1) { if (p.aut) { p.dx += unpx(du); p.dy += unpx(dv); } else { p.ax += unpx(du); p.ay += unpx(dv); } }
            else { int u, v, lu, lv; logical(g_r0, &u, &v, &lu, &lv); p.autosize = 0; p.w = unpx(lu) + unpx(du); p.h = unpx(lv) + unpx(dv); if (p.w < 8) p.w = 8; if (p.h < 8) p.h = 8; }
            el_set_place(g_sel, &p); commit(1); }
        return 0; }
    case WM_LBUTTONUP: if (g_sl >= 0 || g_drag) { g_sl = -1; g_drag = 0; ReleaseCapture(); commit(0); } return 0;
    case WM_MOUSEWHEEL: { int d = -(short)HIWORD(w) / WHEEL_DELTA * px(70); POINT pt = { (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &pt); RECT rc; GetClientRect(h, &rc);
        if (pt.x > rc.right - panel_w()) { g_pscroll += d; if (g_pscroll < 0) g_pscroll = 0; if (g_pscroll > px(700)) g_pscroll = px(700); } else if (g_W * g_s > canvas().right - canvas().left + 1) g_panx += d; else g_pany += d;
        InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_CLOSE: ShowWindow(h, SW_HIDE); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

HWND editor_hwnd(void) { return g_w; }
void editor_open(int tab) {
    if (!g_w) {
        WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(100)); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
        int sw, sh; screen_size(&sw, &sh); int w = px(1120), hh = px(720); if (w > sw - 20) w = sw - 20; if (hh > sh - 60) hh = sh - 60;
        g_w = CreateWindowExW(0, CLASS_NAME, L"WinShell Layout Editor", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX, (sw - w) / 2, (sh - hh) / 2 - px(10), w, hh, NULL, NULL, wc.hInstance, NULL); if (!g_w) return;
    }
    g_tab = tab ? 1 : 0; g_sel = -1; g_zoom = 1.0; g_panx = g_pany = g_pscroll = 0; g_edit_layout = 0;
    ShowWindow(g_w, SW_SHOW); if (IsIconic(g_w)) ShowWindow(g_w, SW_RESTORE); SetForegroundWindow(g_w); InvalidateRect(g_w, NULL, FALSE);
}
void editor_destroy(void) { if (g_w) { DestroyWindow(g_w); g_w = NULL; } }
