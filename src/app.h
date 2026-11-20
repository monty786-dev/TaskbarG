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
void     format_clock(wchar_t *out, int n);

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
void bar_close_all_windows(void);
void bar_show_desktop(void);

/* tray.c */
int  tray_refresh(void);
int  tray_width(void);
void tray_draw(HDC dc, RECT area);
int  tray_click(int x, int y, int button);   /* 0 left, 1 right, 2 double */
void tray_close(void);

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
