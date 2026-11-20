#include "app.h"
#include <math.h>
#include <string.h>

typedef struct {
    const char *name; int dark;
    int bg[3], alt[3], bar[3], start[3], rail[3], accent[3];
    int has_text; int text[3];
} ThemeDef;

static const ThemeDef THEMES[] = {
 {"dark",1,{32,32,32},{43,43,43},{23,23,23},{31,31,31},{20,20,20},{0,120,215},0,{0}},
 {"light",0,{255,255,255},{242,242,242},{240,240,240},{238,238,238},{222,222,222},{0,120,215},0,{0}},
 {"amoled",1,{0,0,0},{16,16,16},{0,0,0},{8,8,8},{0,0,0},{0,153,188},0,{0}},
 {"midnight",1,{18,24,38},{27,36,56},{12,17,28},{18,24,38},{10,14,24},{76,142,255},0,{0}},
 {"ocean",1,{15,40,55},{23,56,75},{10,30,42},{14,38,52},{8,26,36},{0,183,195},0,{0}},
 {"forest",1,{22,36,28},{31,50,39},{15,26,20},{21,34,27},{12,22,16},{16,137,62},0,{0}},
 {"sunset",1,{42,25,29},{58,34,39},{30,18,22},{40,24,28},{26,15,18},{247,99,12},0,{0}},
 {"nord",1,{46,52,64},{59,66,82},{36,41,51},{46,52,64},{33,38,48},{136,192,208},1,{236,239,244}},
 {"rose",0,{255,245,247},{252,232,238},{250,226,234},{255,240,244},{244,214,224},{227,0,140},0,{0}},
 {"lavender",0,{247,244,255},{234,229,250},{228,222,248},{242,238,255},{218,211,242},{135,100,184},0,{0}},
 {"mint",0,{243,255,250},{226,247,238},{216,242,231},{238,252,246},{206,234,222},{0,178,148},0,{0}},
 {"high_contrast",1,{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{255,255,0},1,{255,255,255}},
};

static COLORREF rgb3(const int *v) { return RGB(v[0], v[1], v[2]); }

static COLORREF node_rgb(const JNode *a, COLORREF def) {
    if (!a || a->t != J_ARR) return def;
    int v[3] = { 0, 0, 0 }, i = 0;
    for (const JNode *c = a->child; c && i < 3; c = c->next, i++) v[i] = (int)c->n;
    return i == 3 ? RGB(v[0], v[1], v[2]) : def;
}

void app_spec(Spec *s) {
    const char *name = cfg_str("theme");
    JNode *custom = json_get(cfg_node("custom_themes"), name);
    if (custom && custom->t == J_OBJ) {
        s->bg = node_rgb(json_get(custom, "bg"), RGB(32, 32, 32));
        s->alt = node_rgb(json_get(custom, "alt"), s->bg);
        s->bar = node_rgb(json_get(custom, "bar"), s->bg);
        s->start = node_rgb(json_get(custom, "start"), s->bg);
        s->rail = node_rgb(json_get(custom, "rail"), s->bg);
        s->accent = node_rgb(json_get(custom, "accent"), RGB(0, 120, 215));
        JNode *d = json_get(custom, "dark");
        s->dark = d ? (d->t == J_BOOL ? d->b : d->n != 0) : (c_luminance(s->bg) < 140);
        s->text = node_rgb(json_get(custom, "text"), s->dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
        return;
    }
    const ThemeDef *t = &THEMES[0];
    for (size_t i = 0; i < sizeof THEMES / sizeof THEMES[0]; i++)
        if (!strcmp(THEMES[i].name, name)) { t = &THEMES[i]; break; }
    s->dark = t->dark; s->bg = rgb3(t->bg); s->alt = rgb3(t->alt); s->bar = rgb3(t->bar);
    s->start = rgb3(t->start); s->rail = rgb3(t->rail); s->accent = rgb3(t->accent);
    s->text = t->has_text ? rgb3(t->text) : (t->dark ? RGB(255, 255, 255) : RGB(0, 0, 0));
}

int app_is_dark(void) { Spec s; app_spec(&s); return s.dark; }
COLORREF app_accent(void) { Spec s; app_spec(&s); return c_hex(cfg_str("accent"), s.accent); }

Theme app_theme(int bar) {
    Spec sp; app_spec(&sp);
    int custom = bar && cfg_str("bar_color")[0];
    COLORREF base = custom ? c_hex(cfg_str("bar_color"), sp.bar) : (bar ? sp.bar : sp.start);
    int dark = c_luminance(base) < 140;
    COLORREF text = dark ? RGB(255, 255, 255) : RGB(0, 0, 0);
    Theme t;
    t.bg = base; t.start_bg = sp.start; t.rail = sp.rail;
    t.input = dark ? c_mix(base, text, 0.08) : c_mix(base, RGB(255, 255, 255), 0.78);
    t.hover = c_mix(base, text, 0.14);
    t.active = c_mix(base, text, 0.30);
    t.text = text; t.dim = c_mix(text, base, 0.42);
    t.accent = app_accent(); t.flash = RGB(214, 120, 0);
    return t;
}

int px(double v) {
    double sc = cfg_num("scale"); if (sc <= 0) sc = 1.0;
    int r = (int)floor(v * sc + 0.5);
    return r < 1 ? 1 : r;
}

void screen_size(int *w, int *h) { *w = GetSystemMetrics(SM_CXSCREEN); *h = GetSystemMetrics(SM_CYSCREEN); }

RECT bar_rect(void) {   /* RECT here = {x, y, x+w, y+h} */
    int sw, sh; screen_size(&sw, &sh);
    int h = px(cfg_int("height"));
    RECT r;
    if (!strcmp(cfg_str("position"), "top")) { r.left = 0; r.top = 0; r.right = sw; r.bottom = h; }
    else { r.left = 0; r.top = sh - h; r.right = sw; r.bottom = sh; }
    return r;
}

RECT work_rect(void) {
    int sw, sh; screen_size(&sw, &sh);
    int h = px(cfg_int("height")); RECT r = { 0, 0, sw, sh };
    if (cfg_bool("bar_autohide")) return r;
    if (!strcmp(cfg_str("position"), "top")) r.top = h; else r.bottom = sh - h;
    return r;
}
