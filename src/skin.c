/* Custom title bars drawn on top of other programs' windows.
 * - keeps the bar directly above its window in z-order (event hook + 40 ms check),
 * - moves window and bar together while dragging,
 * - per-button hover colours and a click animation,
 * - optional: removes Wine's own title bar and puts the custom bar above the window. */
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLASS_NAME L"WinShellSkin"
#define MAX_SK 24
enum { B_NONE = -1, B_MIN, B_MAX, B_CLOSE, B_PIN, B_SNAP, B_SHADE, B_COUNT };

typedef struct { const char *name, *label; int left, shape, center, grad; int on[3], off[3], ton[3], toff[3]; } SkinTh;
static const SkinTh TH[] = {
 {"windows10","Windows 10",0,0,0,0,{0,120,215},{235,235,235},{255,255,255},{110,110,110}},
 {"windows11","Windows 11",0,1,0,0,{243,243,243},{250,250,250},{20,20,20},{110,110,110}},
 {"windows8","Windows 8",0,0,0,0,{38,114,196},{210,215,225},{255,255,255},{90,90,90}},
 {"windows7","Windows 7 (Aero)",0,0,0,1,{74,128,196},{190,205,225},{255,255,255},{60,60,60}},
 {"windowsxp","Windows XP (Luna)",0,1,0,1,{0,84,227},{122,150,223},{255,255,255},{216,228,248}},
 {"windows98","Windows 98",0,0,0,1,{0,0,128},{128,128,128},{255,255,255},{212,208,200}},
 {"macos","macOS",1,1,1,0,{236,236,236},{246,246,246},{60,60,60},{150,150,150}},
 {"ubuntu","Ubuntu",0,1,1,0,{51,51,51},{70,70,70},{255,255,255},{170,170,170}},
 {"mint","Linux Mint",0,0,0,0,{60,60,60},{80,80,80},{235,235,235},{160,160,160}},
 {"xfce","Xfce",0,0,0,0,{74,144,217},{190,198,210},{255,255,255},{80,80,80}},
 {"kde","KDE Breeze",0,0,0,0,{49,54,59},{70,76,82},{239,240,241},{150,155,160}},
 {"gnome","GNOME",0,1,1,0,{235,235,235},{245,245,245},{30,30,30},{140,140,140}},
 {"arch","Arch Linux",0,0,0,0,{23,147,209},{50,60,70},{255,255,255},{160,170,180}},
 {"material","Material",0,0,0,0,{33,150,243},{120,144,156},{255,255,255},{230,238,245}},
 {"nord","Nord",0,0,0,0,{59,66,82},{67,76,94},{236,239,244},{150,160,175}},
 {"dark","Dark",0,0,0,0,{32,32,32},{45,45,45},{240,240,240},{140,140,140}},
 {"light","Light",0,0,0,0,{240,240,240},{250,250,250},{20,20,20},{120,120,120}},
};
#define NTH ((int)(sizeof TH / sizeof TH[0]))
static JNode *customs(void) { JNode *o = cfg_node("skin_custom_themes"); return o && o->t == J_OBJ ? o : NULL; }
int skin_theme_count(void) { JNode *c = customs(); return NTH + (c ? json_count(c) : 0); }
const char *skin_theme_name(int i) { if (i < NTH) return TH[i].name; JNode *c = customs(); i -= NTH; for (JNode *k = c ? c->child : NULL; k; k = k->next, i--) if (i == 0 && k->key) return k->key; return "dark"; }
const char *skin_theme_label(int i) { return i < NTH ? TH[i].label : skin_theme_name(i); }
static const char *jstr(JNode *o, const char *k) { JNode *v = o ? json_get(o, k) : NULL; return v && v->t == J_STR && v->s ? v->s : ""; }
static int jbool(JNode *o, const char *k) { JNode *v = o ? json_get(o, k) : NULL; return v && ((v->t == J_BOOL && v->b) || (v->t == J_STR && v->s && (!strcmp(v->s, "yes") || !strcmp(v->s, "round") || !strcmp(v->s, "left")))); }
static void hex3(const char *h, COLORREF def, int *o) { COLORREF c = c_hex(h, def); o[0] = GetRValue(c); o[1] = GetGValue(c); o[2] = GetBValue(c); }
static SkinTh g_ct;
static const SkinTh *cur_theme(void) {
    const char *n = cfg_str("skin_theme"); JNode *c = customs(), *o = c ? json_get(c, n) : NULL;
    if (o) { memset(&g_ct, 0, sizeof g_ct); g_ct.name = n; g_ct.label = n; g_ct.left = jbool(o, "left"); g_ct.shape = jbool(o, "shape"); g_ct.center = jbool(o, "center"); g_ct.grad = jbool(o, "grad");
        hex3(jstr(o, "on"), RGB(0, 120, 215), g_ct.on); hex3(jstr(o, "off"), RGB(235, 235, 235), g_ct.off); hex3(jstr(o, "ton"), RGB(255, 255, 255), g_ct.ton); hex3(jstr(o, "toff"), RGB(110, 110, 110), g_ct.toff); return &g_ct; }
    for (int i = 0; i < NTH; i++) if (!strcmp(TH[i].name, n)) return &TH[i]; return &TH[0];
}

typedef struct {
    HWND target, ov; DWORD orig_style; int style_changed; int hover, down; DWORD click_t; int click_btn;
    HICON ic; wchar_t exe[MAX_PATH]; DWORD ic_t;
    RECT last; wchar_t title[128]; int active, was_zoomed, shaded, drag; POINT drag_start; RECT drag_rect; RECT saved; int snap;
} Skin;
static Skin g_sk[MAX_SK]; static int g_nsk; static int g_tick, g_settle; static HANDLE g_hook1, g_hook2; static int g_inited;

static int hide_native(void) { return cfg_bool("skin_hide_native"); }
static int cap_h_custom(void) { return px(cfg_int("skin_height")); }
static COLORREF C3(const int *v) { return RGB(v[0], v[1], v[2]); }

static Skin *find_skin(HWND t) { for (int i = 0; i < g_nsk; i++) if (g_sk[i].target == t) return &g_sk[i]; return NULL; }
static Skin *find_ov(HWND ov) { for (int i = 0; i < g_nsk; i++) if (g_sk[i].ov == ov) return &g_sk[i]; return NULL; }

static int excluded(HWND h) {
    JNode *ex = cfg_node("skin_exclude"); if (!ex || ex->t != J_ARR || !ex->child) return 0;
    wchar_t pe[MAX_PATH], key[MAX_PATH]; window_process_path(h, pe, MAX_PATH); if (!pe[0]) return 0; app_key(pe, key, MAX_PATH);
    for (JNode *c = ex->child; c; c = c->next) { wchar_t p[MAX_PATH], pe2[MAX_PATH], k2[MAX_PATH]; w_from_utf8_buf(c->s, p, MAX_PATH); exe_for(p, pe2, MAX_PATH); app_key(pe2, k2, MAX_PATH); if (!wcscmp(key, k2)) return 1; }
    return 0;
}
static int eligible(HWND h) {
    if (!IsWindowVisible(h) || IsIconic(h)) return 0;
    LONG_PTR st = GetWindowLongPtrW(h, GWL_STYLE), ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if ((st & WS_CAPTION) != WS_CAPTION && !find_skin(h)) return 0;
    if (ex & WS_EX_TOOLWINDOW) return 0;
    wchar_t cls[96]; GetClassNameW(h, cls, 96);
    if (!wcsncmp(cls, L"WinShell", 8) || !wcscmp(cls, L"Progman") || !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"WorkerW") || !wcscmp(cls, L"#32769") || !wcscmp(cls, L"#32768")) return 0;
    RECT r; GetWindowRect(h, &r); if (r.right - r.left < 120 || r.bottom - r.top < 50) return 0;
    if (GetWindowTextLengthW(h) == 0) return 0;
    DWORD pid = 0; GetWindowThreadProcessId(h, &pid); if (pid == GetCurrentProcessId()) return 0;
    return !excluded(h);
}

/* rectangle the custom bar occupies, in screen coordinates */
static int bar_rect_for(Skin *s, RECT *out) {
    RECT wr; if (!GetWindowRect(s->target, &wr)) return 0;
    if (hide_native() && s->style_changed) { int h = cap_h_custom(); *out = (RECT){ wr.left, wr.top - h, wr.right, wr.top }; return 1; }
    TITLEBARINFO ti; memset(&ti, 0, sizeof ti); ti.cbSize = sizeof ti;
    if (GetTitleBarInfo(s->target, &ti) && ti.rcTitleBar.right > ti.rcTitleBar.left && ti.rcTitleBar.bottom > ti.rcTitleBar.top) { *out = ti.rcTitleBar; out->left = wr.left + (ti.rcTitleBar.left - wr.left > 0 ? ti.rcTitleBar.left - wr.left : 0); return 1; }
    LONG_PTR st = GetWindowLongPtrW(s->target, GWL_STYLE); int fx = GetSystemMetrics((st & WS_THICKFRAME) ? 32 : 7), fy = GetSystemMetrics((st & WS_THICKFRAME) ? 33 : 8), ch = GetSystemMetrics(4);
    if (IsZoomed(s->target)) { fx = 0; fy = 0; }
    *out = (RECT){ wr.left + fx, wr.top + fy, wr.right - fx, wr.top + fy + ch }; return 1;
}

static void place_above(Skin *s) {
    if (!s->ov) return; LONG_PTR ex = GetWindowLongPtrW(s->target, GWL_EXSTYLE); UINT f = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
    int top = (ex & WS_EX_TOPMOST) != 0, ovtop = (GetWindowLongPtrW(s->ov, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    if (top != ovtop) SetWindowPos(s->ov, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, f);
    HWND prev = GetWindow(s->target, GW_HWNDPREV);
    if (prev == s->ov) return;
    if (!prev) { if (top) SetWindowPos(s->ov, HWND_TOPMOST, 0, 0, 0, 0, f); else SetWindowPos(s->ov, HWND_TOP, 0, 0, 0, 0, f); }
    else SetWindowPos(s->ov, prev, 0, 0, 0, 0, f);
}

static void shape_region(Skin *s, int w, int h) {
    const SkinTh *t = cur_theme();
    if (t->shape && !(!hide_native() && !s->style_changed && 0) && cfg_bool("round_corners") && hide_native() && !IsZoomed(s->target)) { int r = px(10) * 2; SetWindowRgn(s->ov, CreateRoundRectRgn(0, 0, w + 1, h + r, r, r), TRUE); }
    else SetWindowRgn(s->ov, NULL, TRUE);
}

static void sync_pos(Skin *s, int force) {
    RECT b; if (!s->ov || !bar_rect_for(s, &b)) return;
    if (!force && EqualRect(&b, &s->last)) return;
    s->last = b; SetWindowPos(s->ov, NULL, b.left, b.top, b.right - b.left, b.bottom - b.top, SWP_NOACTIVATE | SWP_NOZORDER);
    shape_region(s, b.right - b.left, b.bottom - b.top); InvalidateRect(s->ov, NULL, FALSE);
}

static void apply_native(Skin *s) {
    if (!hide_native() || s->style_changed) return;
    LONG_PTR st = GetWindowLongPtrW(s->target, GWL_STYLE); if ((st & WS_CAPTION) != WS_CAPTION) return;
    s->orig_style = (DWORD)st; SetWindowLongPtrW(s->target, GWL_STYLE, (st & ~(LONG_PTR)WS_CAPTION) | WS_THICKFRAME); s->style_changed = 1;
    SetWindowPos(s->target, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
static void restore_native(Skin *s) {
    if (!s->style_changed || !IsWindow(s->target)) { s->style_changed = 0; return; }
    SetWindowLongPtrW(s->target, GWL_STYLE, (LONG_PTR)s->orig_style); s->style_changed = 0;
    SetWindowPos(s->target, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
static void keep_inside(Skin *s) {                       /* hide-native mode: the bar needs room above the window */
    if (!s->style_changed) return; RECT wr, wk = work_rect(); GetWindowRect(s->target, &wr); int h = cap_h_custom();
    if (IsZoomed(s->target)) { if (wr.top < wk.top + h) SetWindowPos(s->target, NULL, wk.left, wk.top + h, wk.right - wk.left, wk.bottom - wk.top - h, SWP_NOACTIVATE | SWP_NOZORDER); }
    else if (wr.top < wk.top + h) SetWindowPos(s->target, NULL, wr.left, wk.top + h, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
}

static LRESULT CALLBACK skin_proc(HWND h, UINT m, WPARAM w, LPARAM l);

static void add_skin(HWND t) {
    if (g_nsk >= MAX_SK) return;
    Skin *s = &g_sk[g_nsk]; memset(s, 0, sizeof *s); s->target = t; s->hover = B_NONE; s->down = B_NONE; s->click_btn = B_NONE;
    apply_native(s); RECT b; if (!bar_rect_for(s, &b)) return;
    s->ov = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, CLASS_NAME, L"", WS_POPUP, b.left, b.top, b.right - b.left, b.bottom - b.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!s->ov) { restore_native(s); return; }
    g_nsk++; sync_pos(s, 1); place_above(s); ShowWindow(s->ov, SW_SHOWNA); keep_inside(s);
}
static void remove_skin(int i) {
    Skin *s = &g_sk[i]; if (s->ov) DestroyWindow(s->ov); restore_native(s); g_sk[i] = g_sk[--g_nsk];
}

/* ---------------------------------------------------------------- buttons */
static int side_left(const SkinTh *t) { const char *v = cfg_str("skin_buttons_side"); if (!strcmp(v, "left")) return 1; if (!strcmp(v, "right")) return 0; return t->left; }
static int circles(const SkinTh *t) { return t->left && strcmp(cfg_str("skin_buttons_side"), "right") != 0; }
static int buttons(Skin *s, int *ids, RECT *rects, int W, int H) {
    const SkinTh *t = cur_theme(); int n = 0, sl = side_left(t), circ = circles(t), bw = circ ? H : (int)(H * 1.45), list[6], k = 0; (void)s;
    if (sl) { list[k++] = B_CLOSE; list[k++] = B_MIN; list[k++] = B_MAX; if (cfg_bool("skin_btn_pin")) list[k++] = B_PIN; if (cfg_bool("skin_btn_snap")) list[k++] = B_SNAP; if (cfg_bool("skin_btn_shade")) list[k++] = B_SHADE; }
    else { if (cfg_bool("skin_btn_shade")) list[k++] = B_SHADE; if (cfg_bool("skin_btn_snap")) list[k++] = B_SNAP; if (cfg_bool("skin_btn_pin")) list[k++] = B_PIN; list[k++] = B_MIN; list[k++] = B_MAX; list[k++] = B_CLOSE; }
    for (int i = 0; i < k; i++) { ids[n] = list[i]; int d = H * 6 / 10;
        if (circ) rects[n] = (RECT){ px(8) + i * (H * 7 / 10 + px(4)), (H - d) / 2, px(8) + i * (H * 7 / 10 + px(4)) + d, (H - d) / 2 + d };
        else if (sl) rects[n] = (RECT){ i * bw, 0, (i + 1) * bw, H };
        else rects[n] = (RECT){ W - (k - i) * bw, 0, W - (k - i - 1) * bw, H };
        n++; }
    return n;
}
static const char *btn_key(int b) { static const char *k[] = { "min", "max", "close", "pin", "snap", "shade" }; return k[b]; }
static COLORREF btn_hover_color(int b, COLORREF cap) {
    char key[40]; snprintf(key, sizeof key, "skin_btn_hover.%s", btn_key(b)); JNode *o = cfg_node("skin_btn_hover"), *v = o ? json_get(o, btn_key(b)) : NULL; (void)key;
    if (v && v->t == J_STR && v->s && v->s[0]) return c_hex(v->s, cap);
    if (b == B_CLOSE) return RGB(232, 17, 35);
    return c_luminance(cap) < 140 ? c_mix(cap, RGB(255, 255, 255), 0.18) : c_mix(cap, RGB(0, 0, 0), 0.12);
}
static COLORREF trafficlight(int b) { return b == B_CLOSE ? RGB(255, 95, 86) : (b == B_MIN ? RGB(255, 189, 46) : RGB(39, 201, 63)); }

static const char *custom_icon(int b, int zoomed) {
    JNode *o = cfg_node("skin_icons"); static const char *k[] = { "min", "max", "close", "pin", "snap", "shade" }; const char *key = (b == B_MAX && zoomed) ? "restore" : k[b];
    JNode *v = o ? json_get(o, key) : NULL; return v && v->t == J_STR && v->s ? v->s : "";
}
static void paint(Skin *s, HDC dc, int W, int H) {
    const SkinTh *t = cur_theme(); int act = s->active; COLORREF cap = act ? C3(t->on) : C3(t->off), txt = act ? C3(t->ton) : C3(t->toff);
    if (cfg_str("skin_color")[0]) { cap = c_hex(cfg_str("skin_color"), cap); txt = c_luminance(cap) < 140 ? RGB(255, 255, 255) : RGB(0, 0, 0); }
    RECT all = { 0, 0, W, H }; if (t->grad) g_vgrad(dc, all, c_shade(cap, 1.18), c_shade(cap, 0.82), 1); else g_fill(dc, all, cap);
    g_fill(dc, (RECT){ 0, H - 1, W, H }, c_mix(cap, c_luminance(cap) < 140 ? RGB(255, 255, 255) : RGB(0, 0, 0), 0.22));
    SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font);
    int ids[6]; RECT br[6]; int nb = buttons(s, ids, br, W, H), lo = px(8), ro = W - px(8);
    for (int i = 0; i < nb; i++) { if (br[i].right < W / 2) { if (br[i].right + px(6) > lo) lo = br[i].right + px(6); } else if (br[i].left - px(6) < ro) ro = br[i].left - px(6); }
    const char *ta = cfg_str("skin_title_align"), *ip = cfg_str("skin_icon_pos");
    int center = !strcmp(ta, "center") || (!strcmp(ta, "auto") && t->center), right = !strcmp(ta, "right");
    int show_icon = strcmp(ip, "off") != 0 && s->ic, icon_title = !strcmp(ip, "title") || (!strcmp(ip, "auto") && center), isz = H * 5 / 9; if (isz > px(20)) isz = px(20); if (isz < 12) isz = 12;
    if (show_icon && !icon_title) { DrawIconEx(dc, lo, (H - isz) / 2, s->ic, isz, isz, 0, NULL, DI_NORMAL); lo += isz + px(8); }
    SIZE sz = { 0, 0 }; GetTextExtentPoint32W(dc, s->title, lstrlenW(s->title), &sz); int extra = (show_icon && icon_title) ? isz + px(6) : 0, avail = ro - lo, tw = sz.cx; if (tw + extra > avail) tw = avail - extra; if (tw < 0) tw = 0;
    int block = tw + extra, x0 = right ? ro - block : (center ? lo + (avail - block) / 2 : lo);
    if (extra) { DrawIconEx(dc, x0, (H - isz) / 2, s->ic, isz, isz, 0, NULL, DI_NORMAL); x0 += extra; }
    g_text(dc, s->title, (RECT){ x0, 0, x0 + tw + 2, H }, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX | DT_LEFT, txt);
    int circ = circles(t);
    for (int i = 0; i < nb; i++) {
        int b = ids[i]; RECT r = br[i]; int hot = s->hover == b, down = s->down == b; const char *ci = custom_icon(b, IsZoomed(s->target)); wchar_t cw[MAX_PATH]; w_from_utf8_buf(ci, cw, MAX_PATH);
        if (circ) {
            COLORREF c = trafficlight(b); if (!act) c = RGB(190, 190, 190); if (down) c = c_shade(c, 0.75);
            HBRUSH hb = CreateSolidBrush(c); HGDIOBJ ob = SelectObject(dc, hb), op = SelectObject(dc, GetStockObject(NULL_PEN)); Ellipse(dc, r.left, r.top, r.right, r.bottom); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(hb);
            RECT gr = { r.left + 2, r.top + 2, r.right - 2, r.bottom - 2 }; if (ci[0] && img_draw(dc, cw, gr, 0, 0)) { } else if (hot) glyph_draw(b == B_CLOSE ? G_CLOSE : (b == B_MIN ? G_MIN : G_MAX), dc, gr, RGB(60, 40, 30));
        } else {
            COLORREF hc = btn_hover_color(b, cap), gc = txt;
            if (hot || down) { g_fill(dc, r, down ? c_shade(hc, 0.82) : hc); if (b == B_CLOSE || c_luminance(hc) < 120) gc = RGB(255, 255, 255); else gc = c_luminance(hc) > 170 ? RGB(20, 20, 20) : gc; }
            int g = b == B_MIN ? G_MIN : (b == B_MAX ? (IsZoomed(s->target) ? G_RESTORE : G_MAX) : (b == B_CLOSE ? G_CLOSE : (b == B_PIN ? G_PIN : (b == B_SNAP ? G_SNAP : G_SHADE))));
            if (b == B_PIN && (GetWindowLongPtrW(s->target, GWL_EXSTYLE) & WS_EX_TOPMOST)) gc = app_accent();
            int sz2 = H * 4 / 10; RECT gr = { (r.left + r.right) / 2 - sz2 / 2, (r.top + r.bottom) / 2 - sz2 / 2, (r.left + r.right) / 2 + sz2 / 2, (r.top + r.bottom) / 2 + sz2 / 2 };
            if (!(ci[0] && img_draw(dc, cw, gr, gc, 0))) glyph_draw(g, dc, gr, gc);
        }
        if (cfg_bool("skin_click_anim") && s->click_btn == b) {
            int age = (int)(GetTickCount() - s->click_t); if (age < 260) { int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, rad = (r.right - r.left) / 2 * age / 200 + 2;
                int alpha = 120 - 120 * age / 260; RECT rr = { cx - rad, cy - rad, cx + rad, cy + rad }; if (rr.left < r.left) rr.left = r.left; if (rr.right > r.right) rr.right = r.right; if (rr.top < r.top) rr.top = r.top; if (rr.bottom > r.bottom) rr.bottom = r.bottom;
                g_alpha(dc, rr, RGB(255, 255, 255), alpha); } else s->click_btn = B_NONE; }
    }
    SelectObject(dc, of);
}

static int btn_at(Skin *s, int x, int y, int W, int H) { int ids[6]; RECT br[6]; int n = buttons(s, ids, br, W, H); for (int i = 0; i < n; i++) if (in_rect(&br[i], x, y)) return ids[i]; return B_NONE; }

static void do_button(Skin *s, int b) {
    HWND t = s->target;
    switch (b) {
    case B_MIN: PostMessageW(t, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case B_MAX: PostMessageW(t, WM_SYSCOMMAND, IsZoomed(t) ? SC_RESTORE : SC_MAXIMIZE, 0); break;
    case B_CLOSE: anim_window(t, 1); break;
    case B_PIN: SetWindowPos(t, (GetWindowLongPtrW(t, GWL_EXSTYLE) & WS_EX_TOPMOST) ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); break;
    case B_SNAP: { RECT wk = work_rect(), r; GetWindowRect(t, &r); if (IsZoomed(t)) ShowWindow(t, SW_RESTORE);
        if (s->snap == 0) { s->saved = r; SetWindowPos(t, NULL, wk.left, wk.top, (wk.right - wk.left) / 2, wk.bottom - wk.top, SWP_NOZORDER | SWP_NOACTIVATE); s->snap = 1; }
        else if (s->snap == 1) { SetWindowPos(t, NULL, (wk.left + wk.right) / 2, wk.top, (wk.right - wk.left) / 2, wk.bottom - wk.top, SWP_NOZORDER | SWP_NOACTIVATE); s->snap = 2; }
        else { SetWindowPos(t, NULL, s->saved.left, s->saved.top, s->saved.right - s->saved.left, s->saved.bottom - s->saved.top, SWP_NOZORDER | SWP_NOACTIVATE); s->snap = 0; } break; }
    case B_SHADE: { RECT r; GetWindowRect(t, &r); if (!s->shaded) { s->saved = r; SetWindowPos(t, NULL, 0, 0, r.right - r.left, GetSystemMetrics(4) + GetSystemMetrics(33) * 2, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); s->shaded = 1; }
        else { SetWindowPos(t, NULL, 0, 0, s->saved.right - s->saved.left, s->saved.bottom - s->saved.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); s->shaded = 0; } break; }
    }
}

static void caption_menu(Skin *s, int x, int y) {
    HWND t = s->target; const wchar_t *lab[] = { L"Restore", L"Minimize", L"Maximize", NULL, L"Close" }; int id[] = { 1, 2, 3, -1, 4 }, fl[] = { IsZoomed(t) ? 0 : MF_GRAYED, 0, IsZoomed(t) ? MF_GRAYED : 0, 0, 0 };
    POINT pt = { x, y }; ClientToScreen(s->ov, &pt); int cmd = app_popup(s->ov, lab, id, fl, 5, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y);
    if (cmd == 1) PostMessageW(t, WM_SYSCOMMAND, SC_RESTORE, 0); else if (cmd == 2) PostMessageW(t, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    else if (cmd == 3) PostMessageW(t, WM_SYSCOMMAND, SC_MAXIMIZE, 0); else if (cmd == 4) anim_window(t, 1);
}

static LRESULT CALLBACK skin_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Skin *s = find_ov(h); int hd; LRESULT mr;
    switch (m) {
    case WM_PAINT: { PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        if (s && rc.right > 0) { HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom); HGDIOBJ old = SelectObject(mem, bmp);
            paint(s, mem, rc.right, rc.bottom); BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); }
        EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: return HTCLIENT;
    case WM_MEASUREITEM: case WM_DRAWITEM: mr = app_menu_msg(m, w, l, &hd); if (hd) return mr; break;
    case WM_TIMER: if (s && w == 1) { InvalidateRect(h, NULL, FALSE); if (GetTickCount() - s->click_t > 280) KillTimer(h, 1); } return 0;
    case WM_MOUSEMOVE: if (!s) break; {
        int x = (short)LOWORD(l), y = (short)HIWORD(l); RECT rc; GetClientRect(h, &rc);
        if (s->drag) { POINT pt; GetCursorPos(&pt); int nx = s->drag_rect.left + pt.x - s->drag_start.x, ny = s->drag_rect.top + pt.y - s->drag_start.y;
            SetWindowPos(s->target, NULL, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            RECT b; if (bar_rect_for(s, &b)) { s->last = b; SetWindowPos(h, NULL, b.left, b.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE); } return 0; }
        int b = btn_at(s, x, y, rc.right, rc.bottom); if (b != s->hover) { s->hover = b; InvalidateRect(h, NULL, FALSE); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); return 0; }
    case WM_MOUSELEAVE: if (s) { s->hover = B_NONE; InvalidateRect(h, NULL, FALSE); } return 0;
    case WM_LBUTTONDOWN: if (!s) break; {
        int x = (short)LOWORD(l), y = (short)HIWORD(l); RECT rc; GetClientRect(h, &rc); SetForegroundWindow(s->target); place_above(s);
        int b = btn_at(s, x, y, rc.right, rc.bottom);
        if (b != B_NONE) { s->down = b; s->click_btn = b; s->click_t = GetTickCount(); SetCapture(h); SetTimer(h, 1, 16, NULL); InvalidateRect(h, NULL, FALSE); }
        else if (!IsZoomed(s->target)) { s->drag = 1; GetCursorPos(&s->drag_start); GetWindowRect(s->target, &s->drag_rect); SetCapture(h); }
        return 0; }
    case WM_LBUTTONUP: if (!s) break; {
        int x = (short)LOWORD(l), y = (short)HIWORD(l); RECT rc; GetClientRect(h, &rc); ReleaseCapture();
        if (s->drag) { s->drag = 0; return 0; }
        int b = btn_at(s, x, y, rc.right, rc.bottom), d = s->down; s->down = B_NONE; InvalidateRect(h, NULL, FALSE);
        if (d != B_NONE && d == b) do_button(s, b); return 0; }
    case WM_LBUTTONDBLCLK: if (s) { int x = (short)LOWORD(l), y = (short)HIWORD(l); RECT rc; GetClientRect(h, &rc); if (btn_at(s, x, y, rc.right, rc.bottom) == B_NONE) PostMessageW(s->target, WM_SYSCOMMAND, IsZoomed(s->target) ? SC_RESTORE : SC_MAXIMIZE, 0); } return 0;
    case WM_RBUTTONUP: if (s) caption_menu(s, (short)LOWORD(l), (short)HIWORD(l)); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------------------------------------ tracking */
static BOOL CALLBACK enum_cb(HWND h, LPARAM lp) { (void)lp; if (!find_skin(h) && eligible(h)) add_skin(h); return TRUE; }

static void sync_one(Skin *s) {
    if (!IsWindow(s->target)) return;
    sync_pos(s, 0); place_above(s);
}
static void CALLBACK ev_proc(HANDLE hook, DWORD ev, HWND h, LONG obj, LONG child, DWORD thr, DWORD time) {
    (void)hook; (void)child; (void)thr; (void)time;
    if (obj != OBJID_WINDOW) return;
    if (ev == EVENT_SYSTEM_FOREGROUND) { g_settle = 8; for (int i = 0; i < g_nsk; i++) sync_one(&g_sk[i]); return; }          /* raise the active window's bar with it */
    Skin *s = find_skin(h); if (s) sync_one(s);
}

void skin_tick(void) {
    if (!g_inited || !cfg_bool("skin_enabled")) return;
    g_tick++;
    if (g_tick % 6 == 0) EnumWindows(enum_cb, 0);
    HWND fg = GetForegroundWindow(); int settle = g_settle > 0; if (settle) g_settle--;
    for (int i = 0; i < g_nsk; ) {
        Skin *s = &g_sk[i];
        if (!IsWindow(s->target) || !IsWindowVisible(s->target) || IsIconic(s->target) || (!s->style_changed && (GetWindowLongPtrW(s->target, GWL_STYLE) & WS_CAPTION) != WS_CAPTION) || excluded(s->target)) {
            if (IsWindow(s->target) && IsWindowVisible(s->target) && !IsIconic(s->target) && s->style_changed) { /* keep */ }
            else { if (!IsWindow(s->target) || IsIconic(s->target) || !IsWindowVisible(s->target)) { ShowWindow(s->ov, SW_HIDE); if (!IsWindow(s->target)) { remove_skin(i); continue; } i++; continue; } remove_skin(i); continue; }
        }
        if (!IsWindowVisible(s->ov)) ShowWindow(s->ov, SW_SHOWNA);
        int act = fg == s->target || (fg && GetAncestor(fg, GA_ROOTOWNER) == s->target);
        wchar_t tt[128]; GetWindowTextW(s->target, tt, 128);
        if (!s->ic || GetTickCount() - s->ic_t > 3000) { if (!s->exe[0]) window_process_path(s->target, s->exe, MAX_PATH); HICON ni = icon_for_window(s->target, s->exe); if (ni != s->ic) { s->ic = ni; InvalidateRect(s->ov, NULL, FALSE); } s->ic_t = GetTickCount(); }
        if (act != s->active || wcscmp(tt, s->title) || IsZoomed(s->target) != s->was_zoomed) { s->active = act; lstrcpyW(s->title, tt); s->was_zoomed = IsZoomed(s->target); InvalidateRect(s->ov, NULL, FALSE); }
        if (hide_native()) { apply_native(s); keep_inside(s); }
        if (!s->drag) sync_one(s); else place_above(s);
        if (settle) place_above(s);
        i++;
    }
}

void skin_apply(void) {
    if (!g_inited) {
        WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.style = CS_DBLCLKS; wc.lpfnWndProc = skin_proc; wc.hInstance = GetModuleHandleW(NULL); wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
        g_hook1 = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, ev_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        g_hook2 = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, ev_proc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        g_inited = 1;
    }
    if (!cfg_bool("skin_enabled")) { while (g_nsk) remove_skin(g_nsk - 1); return; }
    for (int i = 0; i < g_nsk; i++) { if (hide_native()) apply_native(&g_sk[i]); else restore_native(&g_sk[i]); sync_pos(&g_sk[i], 1); InvalidateRect(g_sk[i].ov, NULL, FALSE); }
    EnumWindows(enum_cb, 0);
}
void skin_shutdown(void) {
    while (g_nsk) remove_skin(g_nsk - 1);
    if (g_hook1) UnhookWinEvent(g_hook1); if (g_hook2) UnhookWinEvent(g_hook2); g_hook1 = g_hook2 = NULL; g_inited = 0;
}
