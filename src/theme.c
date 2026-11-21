#include "app.h"
#include <math.h>
#include <stdlib.h>
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
 {"solarized_dark",1,{0,43,54},{7,54,66},{0,34,43},{0,43,54},{0,30,38},{38,139,210},1,{238,232,213}},
 {"solarized_light",0,{253,246,227},{238,232,213},{238,232,213},{253,246,227},{226,219,198},{38,139,210},1,{88,110,117}},
 {"win98",0,{192,192,192},{212,208,200},{192,192,192},{192,192,192},{0,0,128},{0,0,128},1,{0,0,0}},
 {"win7",1,{33,47,64},{44,62,84},{24,38,58},{38,56,80},{28,44,66},{60,140,220},0,{0}},
 {"win8",1,{24,24,24},{38,38,38},{16,16,16},{18,38,82},{12,26,58},{0,120,215},0,{0}},
 {"win11",1,{32,32,32},{44,44,44},{28,28,28},{40,40,40},{32,32,32},{0,103,192},0,{0}},
 {"macos",0,{245,245,247},{232,232,237},{236,236,240},{240,240,244},{224,224,230},{0,122,255},0,{0}},
 {"arch",1,{21,23,26},{30,33,38},{15,17,20},{24,26,31},{12,14,17},{23,147,209},0,{0}},
 {"ubuntu",1,{48,10,36},{70,18,52},{33,0,22},{51,6,38},{28,0,19},{233,84,32},0,{0}},
 {"linuxmint",1,{45,45,45},{58,58,58},{36,36,36},{44,44,44},{30,30,30},{135,207,62},0,{0}},
 {"xfce",1,{59,62,68},{72,76,83},{47,50,55},{56,59,65},{40,43,47},{46,134,193},0,{0}},
 {"kde",1,{49,54,59},{59,66,72},{35,38,41},{42,46,50},{31,34,37},{61,174,233},0,{0}},
 {"gnome",1,{45,45,45},{54,54,54},{29,29,29},{36,36,36},{24,24,24},{53,132,228},0,{0}},
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

int bar_vertical(void) { const char *p = cfg_str("position"); return !strcmp(p, "left") || !strcmp(p, "right"); }

static void bar_margins(int *gap, int *mar) {
    *gap = cfg_bool("bar_float") ? px(cfg_int("bar_gap")) : 0;
    *mar = cfg_bool("bar_float") ? px(cfg_int("bar_margin")) : 0;
}

RECT bar_rect(void) {
    int sw, sh, gap, mar; screen_size(&sw, &sh); bar_margins(&gap, &mar);
    int t = px(cfg_int("height")); const char *pos = cfg_str("position"); RECT r;
    if (bar_vertical()) {
        int len = sh - 2 * mar; if (len < px(200)) len = px(200);
        r.top = mar; r.bottom = mar + len;
        if (!strcmp(pos, "left")) { r.left = gap; r.right = gap + t; } else { r.right = sw - gap; r.left = sw - gap - t; }
    } else {
        int len = sw - 2 * mar; if (len < px(200)) len = px(200);
        r.left = mar; r.right = mar + len;
        if (!strcmp(pos, "top")) { r.top = gap; r.bottom = gap + t; } else { r.bottom = sh - gap; r.top = sh - gap - t; }
    }
    return r;
}

RECT work_rect(void) {
    int sw, sh, gap, mar; screen_size(&sw, &sh); bar_margins(&gap, &mar); (void)mar;
    int t = px(cfg_int("height")) + gap; RECT r = { 0, 0, sw, sh };
    if (cfg_bool("bar_autohide")) return r;
    const char *pos = cfg_str("position");
    if (!strcmp(pos, "top")) r.top = t; else if (!strcmp(pos, "left")) r.left = t; else if (!strcmp(pos, "right")) r.right = sw - t; else r.bottom = sh - t;
    return r;
}

/* ------------------------------------------------------------ OS presets */
typedef struct {
    const char *name, *label, *theme, *skin; int glass, blur, opacity, gloss; const char *align; int height, icon, titles;
    const char *start_style, *bar_style; int bfloat, margin, gap, round, srounded, radius; const char *shape; int anim;
    const char *start_size, *tile_mode; int shuffle; const char *font; int font_size; const char *rail, *start_theme;
} OsDef;
static const OsDef OS[] = {
 {"windows98","Windows 98","win98","windows98",0,0,100,0,"left",32,20,1,"classic","raised",0,0,0,0,0,0,"none",0,"normal","accent",0,"Tahoma",13,"left","windows98"},
 {"windows7","Windows 7","win7","windows7",1,1,82,75,"left",40,26,0,"orb","flat",0,0,0,0,0,0,"none",1,"normal","icon",0,"Segoe UI",14,"left","windows7"},
 {"windows8","Windows 8","win8","windows8",0,0,100,0,"left",40,24,0,"winlogo","flat",0,0,0,0,0,0,"none",1,"full","multi",1,"Segoe UI",14,"right","windows8"},
 {"windows10","Windows 10","dark","windows10",1,1,88,45,"left",40,24,0,"winlogo","flat",0,0,0,0,0,0,"none",1,"normal","multi",0,"Segoe UI",14,"left","windows10"},
 {"windows11","Windows 11","win11","windows11",1,1,90,25,"center",48,26,0,"winlogo","flat",1,0,8,1,1,10,"none",1,"normal","accent",0,"Segoe UI",14,"bottom","windows11"},
 {"macos","macOS","macos","macos",1,1,86,70,"center",52,36,0,"ninebox","flat",1,24,8,1,1,18,"rounded",1,"normal","multi",0,"Segoe UI",14,"bottom","macos"},
 {"arch","Arch Linux","arch","arch",0,0,100,0,"left",34,20,1,"ninebox","flat",0,0,0,0,0,0,"square",0,"normal","accent",0,"Segoe UI",13,"left","arch"},
 {"ubuntu","Ubuntu","ubuntu","ubuntu",0,0,100,0,"left",46,28,0,"ninebox","flat",0,0,0,1,1,10,"circle",1,"normal","multi",0,"Segoe UI",14,"left","ubuntu"},
 {"mint","Linux Mint","linuxmint","mint",0,0,100,0,"left",40,24,0,"ninebox","flat",0,0,0,0,0,0,"rounded",1,"normal","accent",0,"Segoe UI",14,"left","mint"},
 {"xfce","Xfce","xfce","xfce",0,0,100,0,"left",30,20,1,"ninebox","flat",0,0,0,0,0,0,"none",0,"normal","accent",0,"Segoe UI",13,"left","xfce"},
 {"kde","KDE Plasma","kde","kde",1,1,90,20,"left",44,26,0,"ninebox","flat",1,8,6,1,1,8,"none",1,"normal","accent",0,"Segoe UI",14,"left","kde"},
 {"gnome","GNOME","gnome","gnome",0,0,100,0,"center",40,26,0,"ninebox","flat",1,120,8,1,1,14,"rounded",1,"large","multi",0,"Segoe UI",14,"bottom","gnome"},
};
int os_theme_count(void) { return (int)(sizeof OS / sizeof OS[0]); }
const char *os_theme_name(int i) { return OS[i].name; }
const char *os_theme_label(int i) { return OS[i].label; }
void os_theme_apply(const char *name) {
    for (int i = 0; i < os_theme_count(); i++) {
        if (strcmp(OS[i].name, name)) continue;
        const OsDef *o = &OS[i];
        cfg_set_str("os_theme", o->name); cfg_set_str("theme", o->theme); cfg_set_str("skin_theme", o->skin);
        cfg_set_bool("glass", o->glass); cfg_set_bool("glass_blur", o->blur); cfg_set_int("glass_opacity", o->opacity); cfg_set_int("glass_gloss", o->gloss);
        cfg_set_str("alignment", o->align); cfg_set_int("height", o->height); cfg_set_int("icon_size", o->icon); cfg_set_bool("show_titles", o->titles);
        cfg_set_str("start_style", o->start_style); cfg_set_str("bar_style", o->bar_style); cfg_set_bool("bar_float", o->bfloat);
        cfg_set_int("bar_margin", o->margin); cfg_set_int("bar_gap", o->gap ? o->gap : 8); cfg_set_bool("round_corners", o->round);
        cfg_set_bool("start_rounded", o->srounded); cfg_set_int("corner_radius", o->radius); cfg_set_str("icon_shape", o->shape);
        cfg_set_bool("animations", o->anim); cfg_set_str("start_size", o->start_size); cfg_set_str("tile_color_mode", o->tile_mode);
        cfg_set_bool("tile_shuffle", o->shuffle); cfg_set_str("font", o->font); cfg_set_int("font_size", o->font_size);
        cfg_set_str("start_rail_pos", o->rail); cfg_set_str("start_theme", o->start_theme); cfg_set_str("accent", "");
        return;
    }
}

/* --------------------------------------------------- theme list / shuffle */
#include <stdlib.h>
#include <stdio.h>
int theme_count(void) { JNode *c = cfg_node("custom_themes"); return (int)(sizeof THEMES / sizeof THEMES[0]) + (c ? json_count(c) : 0); }
const char *theme_name(int i) {
    int n = (int)(sizeof THEMES / sizeof THEMES[0]); if (i < n) return THEMES[i].name;
    JNode *c = cfg_node("custom_themes"); i -= n;
    for (JNode *k = c ? c->child : NULL; k; k = k->next, i--) if (i == 0 && k->key) return k->key;
    return "dark";
}
static JNode *rgb_arr(COLORREF c) { JNode *a = json_new(J_ARR); json_push(a, json_num(GetRValue(c), 1)); json_push(a, json_num(GetGValue(c), 1)); json_push(a, json_num(GetBValue(c), 1)); return a; }

void shuffle_theme(void) {
    srand(GetTickCount()); double h = rand() % 360, s = 0.25 + (rand() % 40) / 100.0; int dark = rand() % 2;
    JNode *o = json_new(J_OBJ);
    COLORREF bg = dark ? c_hsv(h, s * 0.55, 0.17) : c_hsv(h, s * 0.10, 0.96), alt = dark ? c_hsv(h, s * 0.55, 0.23) : c_hsv(h, s * 0.18, 0.90);
    COLORREF bar = dark ? c_hsv(h, s * 0.65, 0.11) : c_hsv(h, s * 0.15, 0.93), start = dark ? c_hsv(h, s * 0.6, 0.15) : c_hsv(h, s * 0.12, 0.95), rail = dark ? c_hsv(h, s * 0.65, 0.09) : c_hsv(h, s * 0.22, 0.86);
    COLORREF accent = c_hsv(h + (rand() % 2 ? 25 : 170), 0.7 + (rand() % 25) / 100.0, 0.85);
    json_put(o, "dark", json_bool(dark)); json_put(o, "bg", rgb_arr(bg)); json_put(o, "alt", rgb_arr(alt)); json_put(o, "bar", rgb_arr(bar)); json_put(o, "start", rgb_arr(start));
    json_put(o, "rail", rgb_arr(rail)); json_put(o, "accent", rgb_arr(accent)); json_put(o, "text", rgb_arr(dark ? RGB(245, 245, 245) : RGB(25, 25, 25)));
    JNode *ct = cfg_node("custom_themes"); if (ct) json_put(ct, "random", o); else json_free(o);
    cfg_set_str("theme", "random"); cfg_set_str("accent", ""); cfg_set_str("tile_color_mode", "multi");
    cfg_set_str("skin_theme", skin_theme_name(rand() % skin_theme_count()));
    cfg_set_str("ctrl_preset", sys_preset_name(rand() % sys_preset_count())); cfg_set_bool("ctrl_enabled", 1);
    JNode *cc = cfg_node("ctrl_colors"); if (cc) { json_free(cc->child); cc->child = NULL; }
}
