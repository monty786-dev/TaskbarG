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
