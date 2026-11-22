/* Per-element customisation: visible on/off, drag offset, custom icon image and colour.
 * Stored in config.json -> "elements": { "start": {"show":true,"dx":0,"dy":0,"icon":"","color":""} } */
#include "app.h"
#include <string.h>

static const struct { const char *id, *label; } EL[E_COUNT] = {
    { "start", "Start button" }, { "search", "Search box" }, { "desk", "Show desktop button" },
    { "preview", "Window list button" }, { "clock", "Date and time" }, { "tray", "Tray icons" }, { "apps", "Taskbar apps" },
    { "sm_user", "Start: profile" }, { "sm_add", "Start: add app button" }, { "sm_search", "Start: search box" },
    { "sm_power", "Start: power" }, { "sm_settings", "Start: settings" }, { "sm_run", "Start: run" },
    { "sm_programs", "Start: programs" }, { "sm_system", "Start: system" }, { "sm_collapse", "Start: collapse list" },
    { "sm_switch", "Start: tiles / links switch" }, { "sm_tiles", "Start: pinned tiles" },
    { "sm_list", "Start: app list window" }, { "sm_pane", "Start: pinned tiles window" },
};
const char *el_id(int e) { return EL[e].id; }
const char *el_label(int e) { return EL[e].label; }

static JNode *node(int e, int create) {
    JNode *root = cfg_node("elements");
    if (!root || root->t != J_OBJ) return NULL;
    JNode *n = json_get(root, EL[e].id);
    if (!n && create) n = json_put(root, EL[e].id, json_new(J_OBJ));
    return n;
}
static double num(int e, const char *k, double def) { JNode *n = node(e, 0), *v = n ? json_get(n, k) : NULL; return v && v->t == J_NUM ? v->n : def; }
static const char *str(int e, const char *k) { JNode *n = node(e, 0), *v = n ? json_get(n, k) : NULL; return v && v->t == J_STR && v->s ? v->s : ""; }

int el_show(int e) { JNode *n = node(e, 0), *v = n ? json_get(n, "show") : NULL; return v && v->t == J_BOOL ? v->b : 1; }
void el_set_show(int e, int on) { JNode *n = node(e, 1); if (n) json_put(n, "show", json_bool(on)); }
void el_offset(int e, int *dx, int *dy) { *dx = (int)num(e, "dx", 0); *dy = (int)num(e, "dy", 0); }
void el_set_offset(int e, int dx, int dy) { JNode *n = node(e, 1); if (n) { json_put(n, "dx", json_num(dx, 1)); json_put(n, "dy", json_num(dy, 1)); } }
void el_reset_all(void) {
    JNode *root = cfg_node("elements");
    if (!root) return;
    for (int e = 0; e < E_COUNT; e++) { JNode *n = json_get(root, EL[e].id); if (n) { json_put(n, "dx", json_num(0, 1)); json_put(n, "dy", json_num(0, 1)); } }
}
const char *el_icon(int e) { return str(e, "icon"); }
void el_set_icon(int e, const char *path) { JNode *n = node(e, 1); if (n) json_put(n, "icon", json_str(path)); }
int el_has_color(int e) { return str(e, "color")[0] != 0; }
COLORREF el_color(int e, COLORREF def) { return c_hex(str(e, "color"), def); }
void el_set_color(int e, const char *hex) { JNode *n = node(e, 1); if (n) json_put(n, "color", json_str(hex)); }

void el_draw(HDC dc, int e, RECT r, COLORREF def, int glyph) {
    COLORREF col = el_color(e, def); const char *ic = el_icon(e);
    if (ic[0]) { wchar_t w[MAX_PATH]; w_from_utf8_buf(ic, w, MAX_PATH); if (img_draw(dc, w, r, col, el_has_color(e))) return; }
    glyph_draw(glyph, dc, r, col);
}

/* ---- free placement: automatic slot + offset, or absolute position; automatic or explicit size ---- */
static int inum(JNode *n, const char *k, int def) { JNode *v = n ? json_get(n, k) : NULL; return v && v->t == J_NUM ? (int)v->n : def; }
static int ibool(JNode *n, const char *k, int def) { JNode *v = n ? json_get(n, k) : NULL; return v && v->t == J_BOOL ? v->b : def; }
void el_get_place(int e, Place *p) {
    JNode *n = node(e, 0);
    p->dx = inum(n, "dx", 0); p->dy = inum(n, "dy", 0); p->aut = ibool(n, "auto", 1); p->ax = inum(n, "ax", 0); p->ay = inum(n, "ay", 0);
    p->w = inum(n, "w", 0); p->h = inum(n, "h", 0); p->autosize = ibool(n, "autosize", 1);
}
void el_set_place(int e, const Place *p) {
    JNode *n = node(e, 1); if (!n) return;
    json_put(n, "dx", json_num(p->dx, 1)); json_put(n, "dy", json_num(p->dy, 1)); json_put(n, "auto", json_bool(p->aut));
    json_put(n, "ax", json_num(p->ax, 1)); json_put(n, "ay", json_num(p->ay, 1)); json_put(n, "w", json_num(p->w, 1)); json_put(n, "h", json_num(p->h, 1));
    json_put(n, "autosize", json_bool(p->autosize));
}
RECT el_apply(int e, RECT r) {
    Place p; el_get_place(e, &p); int w = r.right - r.left, h = r.bottom - r.top;
    if (!p.autosize) { if (p.w > 0) w = px(p.w); if (p.h > 0) h = px(p.h); }
    int x, y; if (p.aut) { x = r.left + px(p.dx); y = r.top + px(p.dy); } else { x = px(p.ax); y = px(p.ay); }
    RECT o = { x, y, x + w, y + h }; return o;
}
void el_auto_all(void) {
    JNode *root = cfg_node("elements"); if (!root) return;
    for (int e = 0; e < E_COUNT; e++) { JNode *n = json_get(root, EL[e].id); if (!n) continue; Place p; memset(&p, 0, sizeof p); p.aut = 1; p.autosize = 1; el_set_place(e, &p); json_put(n, "show", json_bool(1)); }
}
