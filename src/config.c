#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JNode *g_cfg = NULL;
wchar_t g_config_dir[MAX_PATH];
wchar_t g_config_path[MAX_PATH];

/* kinds: s=string i=int f=float b=bool a=string list ('|' separated) o={} n=null */
typedef struct { const char *key; char kind; const char *s; double n; } Def;

#define S(k, v) { k, 's', v, 0 }
#define I(k, v) { k, 'i', NULL, v }
#define F(k, v) { k, 'f', NULL, v }
#define B(k, v) { k, 'b', NULL, v }
#define A(k, v) { k, 'a', v, 0 }
#define O(k)    { k, 'o', NULL, 0 }
#define N(k)    { k, 'n', NULL, 0 }

static const Def DEFAULTS[] = {
    /* ---- look ---- */
    S("os_theme","windows10"), S("theme","dark"), S("accent",""), S("bar_color",""),
    B("glass",1), I("glass_opacity",88), I("glass_gloss",45), I("glass_blur_amount",14),
    B("glass_blur",1), B("glass_blur_bar",1), B("glass_blur_start",1), B("glass_blur_title",1),
    S("glass_preset","custom"), B("round_corners",0), I("corner_radius",12),
    S("font","Segoe UI"), I("font_size",14), F("scale",1.0),
    S("icon_shape","none"), B("icon_border",0), I("icon_border_width",1), S("icon_border_color",""),
    /* ---- taskbar ---- */
    S("position","bottom"), S("alignment","left"), I("height",40), I("icon_size",24),
    I("button_width",170), B("show_titles",0), B("icon_accent",1),
    B("bar_mirror",0), B("bar_float",0), I("bar_margin",0), I("bar_gap",8), S("bar_style","flat"),
    B("reserve_work_area",1), B("bar_autohide",0), I("reveal_seconds",5),
    B("show_separator",1), B("show_preview_button",1), B("tray_icons",1), B("hide_wine_taskbar",0),
    S("clock_format","%I:%M %p\n%d-%m-%Y"), S("clock_timezone","india"), I("clock_utc_offset",330), I("clock_offset",0),
    B("search_box",0), I("search_w",220), S("search_text","Type here to search"),
    S("start_style","ninebox"), I("start_btn_w",0), I("start_btn_h",0), I("start_icon_scale",100),
    I("desk_w",10), I("clock_w",100), I("tray_cell",24),
    O("elements"),
    /* ---- reveal bar ---- */
    B("strip_enabled",0), S("strip_pos","center"), I("strip_width",40), I("strip_height",6),
    S("strip_color",""), I("strip_opacity",80), I("strip_seconds",4), A("strip_exclude",""),
    B("strip_maximized",0),
    /* ---- start panel ---- */
    S("start_theme","windows10"), S("start_size","normal"), I("tile_level",4),
    I("tile_w_adj",100), I("tile_h_adj",100), I("list_w",290), I("rail_w",48), I("avatar_size",40),
    I("list_row_h",40), I("search_h",32), I("pane_w",230),
    B("start_rounded",0), B("start_list_collapsed",0), B("start_tiles_collapsed",0),
    S("start_rail_pos","left"), S("start_tiles_side","right"), S("start_pane","tiles"),
    B("start_search_in_bar",0), S("start_bg_image",""), I("start_opacity",100),
    B("start_show_recent",1), B("start_show_most_used",1), B("start_programs_mode",0),
    S("tile_color_mode","multi"), O("tile_sizes"), O("tile_colors"), B("tile_shuffle",0), I("tile_shuffle_seconds",8),
    O("app_usage"), A("app_recent",""), A("start_apps",""),
    A("start_pins","explorer.exe|control.exe|cmd.exe|taskmgr.exe|regedit.exe|notepad.exe"),
    A("pane_items","documents|pictures|music|computer|controlpanel|run"),
    O("pane_custom"), S("user_name","User"), S("user_photo",""),
    /* ---- animations ---- */
    B("animations",1), I("anim_ms",160),
    B("anim_bar_on",1), S("anim_bar_dir","auto"), I("anim_bar_ms",220),
    B("anim_start_on",1), S("anim_start_dir","auto"), I("anim_start_ms",180),
    B("anim_app_on",0), S("anim_app_dir","up"), I("anim_app_ms",200),
    B("anim_app_close_on",0), S("anim_app_close_dir","down"), I("anim_app_close_ms",160),
    /* ---- window title bar (skin) ---- */
    B("skin_enabled",1), S("skin_theme","windows10"), O("skin_custom_themes"), S("skin_color_mode","theme"),
    S("skin_color",""), B("skin_hide_native",0), I("skin_height",31),
    B("skin_btn_pin",1), B("skin_btn_snap",0), B("skin_btn_shade",0), B("skin_click_anim",1),
    O("skin_btn_hover"), A("skin_exclude",""),
    /* ---- wine controls (system colours) ---- */
    B("ctrl_enabled",0), S("ctrl_preset","windows10"), O("ctrl_colors"),
    /* ---- theme labs ---- */
    O("custom_themes"),
    /* ---- misc ---- */
    B("autostart",0), A("pinned","explorer.exe"), A("run_history",""),
};

#undef S
#undef I
#undef F
#undef B
#undef A
#undef O
#undef N

static JNode *make_default(const Def *d) {
    switch (d->kind) {
        case 's': return json_str(d->s);
        case 'i': return json_num(d->n, 1);
        case 'f': return json_num(d->n, 0);
        case 'b': return json_bool((int)d->n);
        case 'o': return json_new(J_OBJ);
        case 'n': return json_new(J_NULL);
        case 'a': {
            JNode *arr = json_new(J_ARR);
            if (d->s && *d->s) {
                char *copy = _strdup(d->s), *tok = copy;
                while (tok && *tok) {
                    char *bar = strchr(tok, '|');
                    if (bar) *bar = 0;
                    json_push(arr, json_str(tok));
                    tok = bar ? bar + 1 : NULL;
                }
                free(copy);
            }
            return arr;
        }
    }
    return json_new(J_NULL);
}

static void init_paths(void) {
    wchar_t base[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"PROGRAMDATA", base, MAX_PATH);
    if (!n || n >= MAX_PATH) lstrcpyW(base, L"C:\\ProgramData");
    _snwprintf(g_config_dir, MAX_PATH, L"%ls\\WinShell", base);
    _snwprintf(g_config_path, MAX_PATH, L"%ls\\config.json", g_config_dir);
}

static char *read_file(const wchar_t *path) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 16 * 1024 * 1024) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f); buf[got] = 0; fclose(f);
    return buf;
}

void cfg_load(void) {
    init_paths();
    json_free(g_cfg);
    g_cfg = json_new(J_OBJ);
    for (size_t i = 0; i < sizeof(DEFAULTS) / sizeof(DEFAULTS[0]); i++)
        json_put(g_cfg, DEFAULTS[i].key, make_default(&DEFAULTS[i]));

    CreateDirectoryW(g_config_dir, NULL);
    char *text = read_file(g_config_path);
    JNode *data = text ? json_parse(text) : NULL;
    free(text);
    if (data && data->t == J_OBJ) {
        /* migrations from older versions */
        JNode *cf = json_get(data, "clock_format");
        if (cf && cf->s && strcmp(cf->s, "%H:%M\n%d/%m/%Y") == 0)
            json_put(data, "clock_format", json_str("%I:%M %p\n%d-%m-%Y"));
        JNode *sca = json_get(data, "show_close_all");
        if (sca && !json_get(data, "show_preview_button")) json_put(data, "show_preview_button", json_clone(sca));
        for (JNode *c = data->child; c; c = c->next)
            if (c->key) json_put(g_cfg, c->key, json_clone(c));
    } else {
        cfg_save();   /* first run: write the defaults */
    }
    json_free(data);
}

void cfg_save(void) {
    char *text = json_dump(g_cfg);
    if (!text) return;
    FILE *f = _wfopen(g_config_path, L"wb");
    if (f) { fwrite(text, 1, strlen(text), f); fclose(f); }
    free(text);
}

ULONGLONG cfg_mtime(void) {
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(g_config_path, GetFileExInfoStandard, &fa)) return 0;
    return ((ULONGLONG)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime;
}

JNode *cfg_node(const char *key) { return json_get(g_cfg, key); }
double cfg_num(const char *key) {
    JNode *n = cfg_node(key);
    if (!n) return 0;
    if (n->t == J_NUM) return n->n;
    if (n->t == J_BOOL) return n->b;
    if (n->t == J_STR && n->s) return atof(n->s);
    return 0;
}
int cfg_int(const char *key) { return (int)cfg_num(key); }
int cfg_bool(const char *key) {
    JNode *n = cfg_node(key);
    if (!n) return 0;
    if (n->t == J_BOOL) return n->b;
    if (n->t == J_NUM) return n->n != 0;
    if (n->t == J_STR) return n->s && *n->s;
    return 0;
}
const char *cfg_str(const char *key) {
    JNode *n = cfg_node(key);
    return (n && n->t == J_STR && n->s) ? n->s : "";
}
void cfg_set_str(const char *key, const char *v) { json_put(g_cfg, key, json_str(v)); }
void cfg_set_int(const char *key, int v) { json_put(g_cfg, key, json_num(v, 1)); }
void cfg_set_bool(const char *key, int v) { json_put(g_cfg, key, json_bool(v)); }

int cfg_list_has(const char *key, const char *val) {
    JNode *a = cfg_node(key);
    if (!a || a->t != J_ARR) return 0;
    for (JNode *c = a->child; c; c = c->next) if (c->s && strcmp(c->s, val) == 0) return 1;
    return 0;
}
void cfg_list_set(const char *key, const char **vals, int n) {
    JNode *arr = json_new(J_ARR);
    for (int i = 0; i < n; i++) json_push(arr, json_str(vals[i]));
    json_put(g_cfg, key, arr);
}
