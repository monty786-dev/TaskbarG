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
    /* personalisation */
    S("theme","dark"), B("glass",1), I("glass_opacity",88), S("accent",""), S("bar_color",""),
    S("font","Segoe UI"), I("font_size",14), F("scale",1.0),
    B("glass_blur",1), B("glass_blur_bar",1), B("glass_blur_start",1),
    B("glass_blur_title",1), B("glass_blur_app",0), I("glass_gloss",50),
    S("glass_theme","windows11"), B("corner_round",1), B("animations",1), S("os_preset","custom"),
    /* taskbar layout */
    S("position","bottom"), S("alignment","left"), I("height",40), I("icon_size",24),
    I("button_width",170), B("show_titles",0), B("icon_accent",1),
    B("bar_corner_round",0), I("bar_detach",0), B("auto_bar_width",0),
    S("divider_thickness","mid"), S("icon_shape","square"), B("icon_shape_border",0),
    B("show_separator",1), B("show_preview_button",1), B("tray_icons",1), B("kbd_tray_button",1),
    B("hide_wine_taskbar",0),
    S("clock_format","%I:%M %p\n%d-%m-%Y"), S("clock_timezone","india"), I("clock_utc_offset",330),
    I("clock_offset",0), B("tb_collapse_pinned",0),
    /* start menu */
    S("start_size","normal"), B("start_rounded",0), B("start_list_collapsed",0),
    B("start_show_recent",1), B("start_show_most_used",1),
    I("tile_size",96), O("tile_sizes"), S("tile_color_mode","multi"), B("tile_shuffle",0),
    I("tile_shuffle_seconds",8), O("app_usage"), A("app_recent",""),
    B("show_run_button",1), B("show_programs_button",1), B("show_system_button",1),
    S("power_position","bottom"), S("vbar_bg",""), I("vbar_opacity",100),
    S("start_bg_image",""), I("start_opacity",100),
    /* full-screen / maximise */
    S("fs_mode","resize"), S("max_mode","show"), S("float_corner","bottom-right"), I("reveal_seconds",5),
    /* reveal bar */
    B("bar_autohide",0), B("strip_enabled",0), S("strip_pos","center"),
    I("strip_width",40), I("strip_height",6), S("strip_color",""), I("strip_opacity",80),
    I("strip_seconds",4), A("strip_exclude",""), S("strip_corner","flat"),
    B("strip_fullscreen",1), S("reveal_animation","fade"),
    /* previews */
    B("preview_enabled",1), I("preview_size",200), I("preview_delay",450), B("preview_live",1),
    /* start button */
    S("start_icon",""), S("start_color",""),
    /* window title bar / skin */
    B("skin_enabled",1), S("skin_theme","windows10"), O("skin_custom_themes"),
    S("skin_color_mode","theme"), S("skin_color",""), S("skin_icon_set","theme"),
    B("skin_btn_pin",1), B("skin_btn_snap",1), B("skin_btn_shade",1), S("skin_snap_mode","toggle"),
    B("skin_owned",1), B("skin_border",1), B("skin_scrollbars",1), B("skin_sys_colors",0),
    A("skin_exclude",""), B("skin_disable_native",0), B("skin_override_focused",1),
    B("skin_override_unfocused",1),
    /* wine buttons / progress */
    S("wine_btn_face",""), S("wine_btn_text",""), S("wine_btn_highlight",""),
    S("wine_progress",""), S("wine_accent",""),
    /* theme labs */
    O("custom_themes"),
    S("lab_shell_name",""), S("lab_shell_base","dark"), S("lab_shell_bg",""), S("lab_shell_alt",""),
    S("lab_shell_bar",""), S("lab_shell_start",""), S("lab_shell_rail",""), S("lab_shell_accent",""),
    S("lab_shell_text",""),
    S("lab_win_name",""), S("lab_win_layout",""), S("lab_win_shape",""), S("lab_win_glyph",""),
    S("lab_win_align",""), S("lab_win_cap",""), S("lab_win_grad",""), S("lab_win_cap_off",""),
    S("lab_win_text",""), S("lab_win_text_off",""), S("lab_win_border",""), S("lab_win_border_off",""),
    S("lab_win_hover",""), S("lab_win_base","windows10"),
    S("lab_img_min",""), S("lab_img_max",""), S("lab_img_restore",""), S("lab_img_close",""),
    S("lab_img_pin",""), S("lab_img_snap",""), S("lab_img_shade",""),
    /* cursors */
    B("cursor_enabled",0), S("cursor_preset","windows10"),
    S("cursor_arrow",""), S("cursor_ibeam",""), S("cursor_wait",""), S("cursor_appstarting",""),
    S("cursor_cross",""), S("cursor_hand",""), S("cursor_help",""), S("cursor_no",""),
    S("cursor_sizeall",""), S("cursor_sizens",""), S("cursor_sizewe",""), S("cursor_sizenwse",""),
    S("cursor_sizenesw",""), S("cursor_uparrow",""), S("cursor_grab",""), S("cursor_grabbing",""),
    /* keyboard */
    B("kbd_auto",0), B("kbd_use_android",1), I("kbd_height",38), I("kbd_opacity",96),
    B("kbd_pan",1), I("kbd_hide_delay",2), A("kbd_exclude",""),
    /* file manager */
    B("builtin_filemanager",1), S("fm_view","details"), B("fm_show_hidden",0),
    I("fm_icon_size",64), A("fm_favorites",""), A("fm_bookmarks",""), N("fm_home"), N("fm_pc"),
    O("fm_folder_icons"), A("fm_open_with",""), O("fm_assoc"), A("fm_pins",""),
    /* desktop */
    B("full_desktop",0), I("grid_size",96), B("grid_snap",1), B("show_desktop_icons",1),
    S("wallpaper",""), S("wallpaper_mode","fill"), S("desktop_color","#1E3A5F"),
    A("desktop_extra",""), A("desktop_hidden",""), O("desktop_layout"), B("desktop_autopin",1),
    /* startup */
    B("autostart",0),
    /* apps */
    A("pinned","explorer.exe"), A("start_apps",""),
    A("start_pins","explorer.exe|control.exe|cmd.exe|taskmgr.exe|regedit.exe|notepad.exe"),
    S("user_name","User"), S("user_photo",""), B("reserve_work_area",1),
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
        JNode *lock = json_get(data, "lock_taskbar");
        if (lock && !json_get(data, "fs_mode"))
            json_put(data, "fs_mode", json_str(lock->b ? "resize" : "hide"));
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
