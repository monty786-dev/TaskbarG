/* WinShell v7 - Taskbar & Desktop (C port)
 * Original Python version: https://github.com/monty786-dev/Winlator-Wine-Look-changer */
#ifndef WS_APP_H
#define WS_APP_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <winreg.h>
#include <shellapi.h>
#include <wchar.h>
#include <time.h>
#include "json.h"

/* ---------------------------------------------------------------- config */
extern JNode *g_cfg;
extern wchar_t g_config_dir[MAX_PATH];
extern wchar_t g_config_path[MAX_PATH];

void       cfg_load(void);
void       cfg_save(void);
ULONGLONG  cfg_mtime(void);
JNode     *cfg_node(const char *key);
int        cfg_int(const char *key);
double     cfg_num(const char *key);
int        cfg_bool(const char *key);
const char*cfg_str(const char *key);               /* UTF-8, "" if missing */
void       cfg_set_str(const char *key, const char *val);
void       cfg_set_int(const char *key, int val);
void       cfg_set_bool(const char *key, int val);
int        cfg_list_has(const char *key, const char *val);
void       cfg_list_set(const char *key, const char **vals, int n);

/* ------------------------------------------------------------------ util */
#define IST_OFFSET 19800
wchar_t *w_from_utf8(const char *s);                 /* malloc'd */
void     w_from_utf8_buf(const char *s, wchar_t *out, int n);
char    *utf8_from_w(const wchar_t *s);              /* malloc'd */

COLORREF c_shade(COLORREF c, double f);
COLORREF c_mix(COLORREF a, COLORREF b, double t);
double   c_luminance(COLORREF c);
COLORREF c_hex(const char *text, COLORREF def);
COLORREF c_hsv(double h, double s, double v);
void     format_clock(wchar_t *out, int n);
void     clock_tm(struct tm *out);

/* ----------------------------------------------------------------- theme */
typedef struct {
    int dark;
    COLORREF bg, alt, bar, start, rail, accent, text;
} Spec;
typedef struct {
    COLORREF bg, start_bg, rail, input, hover, active, text, dim, accent, flash;
} Theme;

void     app_spec(Spec *out);
int      app_is_dark(void);
COLORREF app_accent(void);
Theme    app_theme(int bar);                         /* bar=1 taskbar, 0 start */
int      px(double v);
int      bar_vertical(void);
int      os_theme_count(void);
const char *os_theme_name(int i);
const char *os_theme_label(int i);
void     os_theme_apply(const char *name);
RECT     bar_rect(void);
RECT     work_rect(void);
void     screen_size(int *w, int *h);

/* ------------------------------------------------------------------- gdi */
extern HFONT g_font, g_font_big, g_font_small;
void apply_fonts(void);
void g_fill(HDC dc, RECT r, COLORREF c);
void g_frame(HDC dc, RECT r, COLORREF c, int t);
void g_alpha(HDC dc, RECT r, COLORREF c, int alpha);
void g_vgrad(HDC dc, RECT r, COLORREF c1, COLORREF c2, int step);
void g_text(HDC dc, const wchar_t *s, RECT r, UINT flags, COLORREF c);
void g_glass_blit(HDC dc, int slot, int w, int h, COLORREF base); /* slot 0=bar 1=start */
void g_glass_clear(void);
void apply_glass(HWND h, int on, int pct);
int  in_rect(const RECT *r, int x, int y);

/* -------------------------------------------------------- glyphs / elements */
enum { G_NINEBOX, G_WINLOGO, G_POWER, G_GEAR, G_RUN, G_PROGRAMS, G_SYSTEM, G_COLLAPSE, G_SEARCH, G_USER, G_DESK,
       G_PREVIEW, G_PLUS, G_SWITCH, G_CHEVRON_R, G_CHEVRON_D, G_FOLDER, G_DOC, G_PIC, G_MUSIC, G_PC, G_PANEL, G_HELP,
       G_CLOSE, G_MIN, G_MAX, G_RESTORE, G_PIN, G_SNAP, G_SHADE, G_PLAY_, G_COUNT };
void glyph_draw(int g, HDC dc, RECT r, COLORREF c);
int  img_draw(HDC dc, const wchar_t *path, RECT r, COLORREF tint, int use_tint);
void img_clear(void);

enum { E_START, E_SEARCH, E_DESK, E_PREVIEW, E_CLOCK, E_TRAY, E_MODE, E_APPS,
       E_SM_USER, E_SM_ADD, E_SM_SEARCH, E_SM_POWER, E_SM_SETTINGS, E_SM_RUN, E_SM_PROGRAMS, E_SM_SYSTEM,
       E_SM_COLLAPSE, E_SM_SWITCH, E_SM_TILES, E_SM_LIST, E_SM_PANE, E_SM_MODE, E_COUNT };
const char *el_id(int e);
const char *el_label(int e);
int      el_show(int e);
void     el_set_show(int e, int on);
void     el_offset(int e, int *dx, int *dy);
void     el_set_offset(int e, int dx, int dy);
void     el_reset_all(void);
const char *el_icon(int e);
void     el_set_icon(int e, const char *path);
int      el_has_color(int e);
COLORREF el_color(int e, COLORREF def);
void     el_set_color(int e, const char *hex);
void     el_draw(HDC dc, int e, RECT r, COLORREF def, int glyph);
typedef struct { int dx, dy, aut, ax, ay, w, h, autosize; } Place;   /* aut: follow the automatic layout; autosize: automatic size */
void     el_get_place(int e, Place *p);
void     el_set_place(int e, const Place *p);
RECT     el_apply(int e, RECT autor);          /* automatic rectangle -> final rectangle (offset / absolute / resized) */
void     el_auto_all(void);                    /* everything back to automatic position, size and visibility */
void     g_icon_a(HDC dc, HICON ic, int x, int y, int size, int alpha_pct);
int      icon_is_generic(HICON ic);
void     g_icon_name(HDC dc, HICON ic, const wchar_t *name, int x, int y, int size, int alpha_pct);   /* icon, or a colour-filled first-letter badge */
void     g_badge(HDC dc, const wchar_t *name, RECT r, COLORREF fill);
COLORREF pill_color(const wchar_t *text);
RECT     g_pill(HDC dc, RECT slot, const wchar_t *text, int align, int active);   /* text-sized colour-filled shape; returns drawn rect */
HICON    img_to_icon(const wchar_t *path, int size);
int      img_cover(HDC dc, const wchar_t *path, RECT r, int alpha_pct);
void     app_backup_save(HWND owner);
void     app_backup_restore(HWND owner);
void     app_toggle_desktop(void);
void     shuffle_os(int everything);
void     editor_open(int tab);
HWND     editor_hwnd(void);
void     editor_destroy(void);
int      preview_visible(void);
HWND     preview_hwnd(void);
void     bar_render(HDC dc, int W, int H);
int      bar_element_rects(int *ids, RECT *rc, int max, int *W, int *H);
void     start_render(HDC dc, int W, int H);
int      start_element_rects(int *ids, RECT *rc, int max, int *W, int *H);
COLORREF g_mixa(COLORREF bg, COLORREF fg, int alpha_pct);
int      img_fill(HDC dc, const wchar_t *path, int W, int H, const char *mode);   /* wallpaper */
int      img_fill_a(HDC dc, const wchar_t *path, int W, int H, const char *mode, int alpha_pct);
JNode   *fake_app(const wchar_t *path);

/* ---------------------------------------------------------- wine colours */
int  sys_preset_count(void);
const char *sys_preset_name(int i);
const char *sys_preset_label(int i);
int  sys_slot_count(void);
const char *sys_slot_key(int i);
const char *sys_slot_label(int i);
const char *sys_slot_group(int i);
COLORREF sys_slot_color(int i);
int  sys_slot_overridden(int i);
void sys_slot_set(int i, const char *hex);
void sys_preset_apply(int i);
void sys_apply(void);
void sys_restore(void);

/* ------------------------------------------------------------ misc modules */
int  theme_count(void);
const char *theme_name(int i);
void shuffle_theme(void);
void shuffle_tiles(void);
int  skin_theme_count(void);
const char *skin_theme_name(int i);
const char *skin_theme_label(int i);
void skin_init(void);
void skin_apply(void);
void skin_tick(void);
void skin_shutdown(void);
void reveal_apply(void);
void reveal_show(int on);
void reveal_destroy(void);
int  pane_link_count(void);
const char *pane_link_id(int i);
const char *pane_link_label(int i);
int  pane_link_glyph(int i);
void settings_open(int page);
HWND settings_hwnd(void);
void settings_refresh(void);
void settings_destroy(void);
extern int g_edit_layout;                 /* 0 off, 1 taskbar, 2 start panel */
LRESULT app_menu_msg(UINT msg, WPARAM wp, LPARAM lp, int *handled);
void apply_alpha(HWND h, int alpha255);

/* ------------------------------------------------------------- animation */
#define ANIM_END_NONE  0
#define ANIM_END_HIDE  1
#define ANIM_END_CLOSE 2
void  anim_run(HWND h, RECT from, RECT to, int ms, int a_from, int a_to, int end, RECT restore);
void  anim_cancel(HWND h);
int   anim_busy(HWND h);
RECT  anim_offset(RECT r, const char *dir, int dist);
const char *anim_edge_dir(const char *pos);
void  anim_window(HWND h, int closing);

/* ----------------------------------------------------------------- icons */
HICON  icon_for_window(HWND h, const wchar_t *exe);
HICON  icon_for_file(const wchar_t *path);
COLORREF icon_accent(HICON ic, COLORREF def);
void   icons_close(void);

/* ----------------------------------------------------------------- paths */
wchar_t *find_exe(const wchar_t *name, wchar_t *out, int n);
void     exe_for(const wchar_t *path, wchar_t *out, int n);   /* resolves .lnk */
void     app_key(const wchar_t *path, wchar_t *out, int n);   /* lower basename */
void     display_name(const wchar_t *path, wchar_t *out, int n);
void     window_process_path(HWND h, wchar_t *out, int n);
int      lnk_info(const wchar_t *path, wchar_t *target, wchar_t *icon, int *index, int n);
int      path_exists(const wchar_t *p);
int      path_isdir(const wchar_t *p);

/* ------------------------------------------------------------------- app */
#define MAX_ENTRIES 64
#define MAX_TASKS   32
#define POPUP_CUR   (-100000)

typedef struct {
    wchar_t key[MAX_PATH];
    wchar_t path[MAX_PATH];
    wchar_t title[256];
    int pinned;
    HWND tasks[MAX_TASKS];
    int ntasks;
    HICON hicon;
    COLORREF accent; int has_accent;
    DWORD icon_time;
} Entry;

extern HWND g_bar_hwnd;
extern int  g_menu_depth;

/* taskbar.c */
int  bar_create(void);
void bar_apply(void);
void bar_restore_work_area(void);
void bar_rebuild_pins(void);
void bar_refresh(int force);
void bar_invalidate(void);
int  bar_start_left(void);
int  bar_start_right(void);
void bar_pin(const wchar_t *path);
void bar_unpin(const wchar_t *key);
HWND bar_hwnd(void);
int  bar_is_hidden(void);
RECT bar_start_screen_rect(void);
int  bar_fg_excluded(void);
void bar_reveal(int seconds);
void settings_open_page(const char *name);
const wchar_t *start_query(void);
void start_open_search(void);
void bar_close_all_windows(void);
void bar_show_desktop(void);

/* tray.c */
int  tray_refresh(void);
int  tray_width(void);
void tray_draw(HDC dc, RECT area, int vertical);
void tray_add_app(const wchar_t *path);
int  tray_click(int x, int y, int button);   /* 0 left, 1 right, 2 double */
void tray_close(void);

/* desktop.c / gamemode.c */
void desktop_apply(void);
void desktop_tick(void);
void desktop_sync(void);
void desktop_destroy(void);
void desktop_add_shortcut(const wchar_t *path, const char *type);
void desktop_add_widget(const char *kind);
void desktop_add_image(const wchar_t *img, const wchar_t *target);
void desktop_arrange(void);
void desktop_clear(void);
void game_open(void);
void game_close(void);
void game_apply(void);
void game_destroy(void);
int  game_is_open(void);

/* preview.c */
int  bar_task_windows(HWND *out, int max);
void preview_toggle(void);
void preview_close(void);
void preview_destroy(void);

/* startmenu.c */
extern int g_start_visible;
void start_toggle(void);
void start_close(void);
void start_apply(void);
void start_destroy(void);
HWND start_hwnd(void);

/* app.c */
void app_run(const wchar_t *target, const wchar_t *params, int show);
void app_reload(void);
void app_apply_all(void);
void app_shutdown(void);
void app_save(void);
ULONGLONG app_config_mtime(void);
void app_restart(void);
int  app_popup(HWND owner, const wchar_t **labels, const int *ids, const int *flags, int n,
               UINT tpm_flags, int x, int y);
int  app_confirm(HWND owner, const wchar_t *title, const wchar_t *text);
void app_record_use(const char *path);

#endif
