/* WinShell settings window: themed, Windows 10 style, declarative pages. */
#include "app.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLASS_NAME L"WinShellSettings"
#define MAX_ITEMS 420

enum { D_HEAD, D_INFO, D_BOOL, D_SLIDER, D_FSLIDER, D_CHOICE, D_COLOR, D_STR, D_FILE, D_BUTTON, D_LISTROW, D_LISTADD,
       D_ELEM, D_SYSCLR, D_PLINK };
enum { A_OS_APPLY = 1, A_SHUFFLE_THEME, A_SHUFFLE_TILES, A_EDIT_TASKBAR, A_EDIT_START, A_RESET_LAYOUT, A_OPEN_CFG,
       A_RELOAD, A_RESTART, A_SYS_RESET, A_PANE_RESET, A_RESET_ELEMS, A_DESK_ADD, A_DESK_TILE, A_DESK_CLOCK, A_DESK_CAL, A_DESK_SYS, A_DESK_ARRANGE,
       A_DESK_SYNC, A_DESK_CLEAR, A_GAME_OPEN, A_GAME_ADD, A_RANDOM_OS, A_RANDOM_ALL, A_PILL_SHUFFLE, A_BACKUP_SAVE, A_BACKUP_RESTORE, A_SKIN_SAVE, A_SKIN_DEL, A_FAKE_ADD, A_DESK_IMAGE,
       A_GAME_DEL = 1000, A_FAKE_BASE = 2000, A_BUILTIN_BASE = 3000 };

typedef struct { int type; const char *key, *label; int a, b; const char *opts; } Def;
typedef struct { int type; char key[64]; char label[170]; char opts[900]; int a, b; int y, h; } Item;
typedef struct { const char *group, *name; const Def *defs; int n; void (*build)(void); } Page;

static HWND g_hwnd, g_edit;
static WNDPROC g_edit_old;
static Item g_items[MAX_ITEMS]; static int g_nitems;
static int g_page, g_scroll, g_nav_scroll, g_content_h;
static int g_hover = -1, g_hover_part = -1, g_drag = -1, g_nav_hover = -1, g_edit_item = -1;
static DWORD g_last_live;

#define H_(t) { D_HEAD, "", t, 0, 0, NULL }
#define I_(t) { D_INFO, "", t, 0, 0, NULL }
#define B_(k, t) { D_BOOL, k, t, 0, 0, NULL }
#define S_(k, t, lo, hi) { D_SLIDER, k, t, lo, hi, NULL }
#define F_(k, t, lo, hi) { D_FSLIDER, k, t, lo, hi, NULL }
#define C_(k, t, o) { D_CHOICE, k, t, 0, 0, o }
#define K_(k, t) { D_COLOR, k, t, 0, 0, NULL }
#define T_(k, t) { D_STR, k, t, 0, 0, NULL }
#define P_(k, t, f) { D_FILE, k, t, 0, 0, f }
#define X_(id, t) { D_BUTTON, "", t, id, 0, NULL }
#define L_(k, t) { D_LISTADD, k, t, 0, 0, NULL }

/* ------------------------------------------------------------------ values */
/* keys may be nested: "skin_btn_hover.min", "games.2.name" */
static JNode *resolve(const char *key, const char **leaf) {
    char tok[64]; JNode *cur = g_cfg; const char *p = key;
    for (;;) {
        const char *dot = strchr(p, '.'); size_t len = dot ? (size_t)(dot - p) : strlen(p); if (len > 63) len = 63; memcpy(tok, p, len); tok[len] = 0;
        if (!dot) { *leaf = p; return cur; }
        JNode *nx = NULL;
        if (cur && cur->t == J_ARR) { int idx = atoi(tok); nx = cur->child; while (nx && idx-- > 0) nx = nx->next; } else nx = json_get(cur, tok);
        if (!nx) return NULL; cur = nx; p = dot + 1;
    }
}
static const char *get_str(const char *key) {
    const char *leaf; JNode *par = resolve(key, &leaf); if (!par) return "";
    JNode *v = par->t == J_ARR ? NULL : json_get(par, leaf); return v && v->t == J_STR && v->s ? v->s : "";
}
static void set_str(const char *key, const char *val) {
    const char *leaf; JNode *par = resolve(key, &leaf); if (!par || par->t != J_OBJ) return;
    if (!strchr(key, '.') || (val && val[0]) || !strncmp(key, "games.", 6)) json_put(par, leaf, json_str(val ? val : "")); else json_remove(par, leaf);   /* empty nested colours fall back to the theme */
}
static void to_hex(COLORREF c, char *out) { snprintf(out, 8, "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c)); }

static void changed(void) { app_save(); app_apply_all(); settings_refresh(); }

/* --------------------------------------------------------------- page build */
static Item *add(int type, const char *key, const char *label, int a, int b, const char *opts) {
    if (g_nitems >= MAX_ITEMS) return NULL;
    Item *it = &g_items[g_nitems++]; memset(it, 0, sizeof *it);
    it->type = type; it->a = a; it->b = b;
    lstrcpynA(it->key, key ? key : "", 64); lstrcpynA(it->label, label ? label : "", 170); lstrcpynA(it->opts, opts ? opts : "", 900);
    return it;
}
static void add_defs(const Def *d, int n) { for (int i = 0; i < n; i++) add(d[i].type, d[i].key, d[i].label, d[i].a, d[i].b, d[i].opts); }
static void add_list(const char *key, const char *label, const char *filter) {
    JNode *arr = cfg_node(key); int i = 0;
    for (JNode *c = arr ? arr->child : NULL; c; c = c->next, i++) { Item *it = add(D_LISTROW, key, c->s ? c->s : "", i, 0, filter); (void)it; }
    add(D_LISTADD, key, label, 0, 0, filter);
}

static char *join_opts(char *buf, int n, const char *prefix, int count, const char *(*name)(int), const char *(*label)(int)) {
    buf[0] = 0; int len = 0; (void)prefix;
    for (int i = 0; i < count; i++) {
        int w = snprintf(buf + len, (size_t)(n - len), "%s%s=%s", i ? "|" : "", name(i), label ? label(i) : name(i));
        if (w < 0 || len + w >= n - 1) break; len += w;
    }
    return buf;
}

static void build_os(void) {
    static char o[900]; join_opts(o, 900, "", os_theme_count(), os_theme_name, os_theme_label);
    add(D_INFO, "", "One choice restyles the taskbar, Start panel, window title bars, fonts and glass. Pick an operating system and press Apply.", 0, 0, NULL);
    add(D_CHOICE, "os_theme", "Operating system theme", 0, 0, o);
    add(D_BUTTON, "", "Apply this OS theme", A_OS_APPLY, 0, NULL);
}
static void build_personal(void) {
    static char o[900]; o[0] = 0; int len = 0;
    for (int i = 0; i < theme_count(); i++) len += snprintf(o + len, sizeof o - (size_t)len, "%s%s", i ? "|" : "", theme_name(i));
    { static char oo[900]; join_opts(oo, 900, "", os_theme_count(), os_theme_name, os_theme_label); add(D_HEAD, "", "Operating system look", 0, 0, NULL); add(D_CHOICE, "os_theme", "OS theme", 0, 0, oo);
      add(D_BUTTON, "", "Apply this OS theme", A_OS_APPLY, 0, NULL); add(D_BUTTON, "", "Random OS theme", A_RANDOM_OS, 0, NULL); add(D_BUTTON, "", "Random everything: OS, colours, shapes, title bars, game mode", A_RANDOM_ALL, 0, NULL); }
    add(D_HEAD, "", "Theme", 0, 0, NULL); add(D_CHOICE, "theme", "Theme", 0, 0, o);
    add(D_COLOR, "accent", "Accent colour (empty = theme default)", 0, 0, NULL);
    add(D_HEAD, "", "Text and size", 0, 0, NULL); add(D_STR, "font", "Font name", 0, 0, NULL);
    add(D_SLIDER, "font_size", "Font size", 8, 32, NULL); add(D_FSLIDER, "scale", "UI scale (%)", 50, 300, NULL);
    add(D_HEAD, "", "Random", 0, 0, NULL);
    add(D_BUTTON, "", "Shuffle theme: every colour, title bar style and Wine controls", A_SHUFFLE_THEME, 0, NULL);
    add(D_BUTTON, "", "Shuffle pinned tiles: sizes and colours", A_SHUFFLE_TILES, 0, NULL);
}
static void build_start_theme(void) {
    add(D_INFO, "", "Each style recreates the layout of that system's Start menu: tiles, two-column with links, or an icon grid.", 0, 0, NULL);
    add(D_CHOICE, "start_theme", "Start panel style", 0, 0, "windows10=Windows 10|windows8=Windows 8 (full screen tiles)|windows11=Windows 11|windows7=Windows 7|windowsxp=Windows XP|windows98=Windows 98|zorin=Zorin OS|macos=macOS|gnome=GNOME|ubuntu=Ubuntu|mint=Linux Mint|kde=KDE Plasma|xfce=Xfce|arch=Arch Linux");
}
static void build_elems(int from, int to, const char *note) {
    add(D_INFO, "", note, 0, 0, NULL);
    for (int e = from; e <= to; e++) add(D_ELEM, el_id(e), el_label(e), e, 0, NULL);
    add(D_BUTTON, "", "Auto-adapt all elements (position, size, visibility)", A_RESET_ELEMS, 0, NULL);
}
static void build_layout_editor(void) {
    add(D_INFO, "", "Drag any taskbar element with the mouse to move it; positions are kept relative to the bar so they adapt when the bar height or position changes. Turn off an element with its switch, give it your own .png / .ico / image and pick any colour.", 0, 0, NULL);
    add(D_BUTTON, "", "Open the layout editor window (add, delete, resize, icon, colour)", A_EDIT_TASKBAR, 0, NULL);
    build_elems(E_START, E_APPS, "Taskbar elements, including the running-app icons and titles (group). In edit mode drag to move, drag the corner square to resize, right-click for Auto position / Auto size.");
}
static void build_start_elems(void) {
    add(D_BUTTON, "", "Open the layout editor window for the Start panel", A_EDIT_START, 0, NULL);
    build_elems(E_SM_USER, E_SM_MODE, "Start panel elements and windows: on / off, icon image, colour. In edit mode drag to move, drag the corner square to resize, right-click for Auto position / Auto size.");
}
static void build_pane(void) {
    add(D_INFO, "", "Links shown in the right pane (Windows 7 / Zorin style). Use the switch to show or hide, arrows to reorder, then icon and colour.", 0, 0, NULL);
    JNode *shown = cfg_node("pane_items");
    for (JNode *c = shown ? shown->child : NULL; c; c = c->next) for (int i = 0; i < pane_link_count(); i++)
        if (c->s && !strcmp(c->s, pane_link_id(i))) add(D_PLINK, pane_link_id(i), pane_link_label(i), i, 1, NULL);
    for (int i = 0; i < pane_link_count(); i++) if (!cfg_list_has("pane_items", pane_link_id(i))) add(D_PLINK, pane_link_id(i), pane_link_label(i), i, 0, NULL);
    add(D_BUTTON, "", "Reset links to default", A_PANE_RESET, 0, NULL);
}
static void build_sys(void) {
    static char o[900]; join_opts(o, 900, "", sys_preset_count(), sys_preset_name, sys_preset_label);
    add(D_INFO, "", "Colours Wine uses for windows, buttons, checkboxes, menus, tooltips, scroll bars and selection. Applied live and saved to the registry for new programs. Wine has no per-control hover colour; hot-tracking and menu highlight are the closest.", 0, 0, NULL);
    add(D_BOOL, "ctrl_enabled", "Use these colours", 0, 0, NULL);
    add(D_CHOICE, "ctrl_preset", "OS colour preset", 0, 0, o);
    const char *last = "";
    for (int i = 0; i < sys_slot_count(); i++) {
        if (strcmp(last, sys_slot_group(i))) { add(D_HEAD, "", sys_slot_group(i), 0, 0, NULL); last = sys_slot_group(i); }
        add(D_SYSCLR, "", sys_slot_label(i), i, 0, NULL);
    }
    add(D_BUTTON, "", "Clear my colour changes (back to the preset)", A_SYS_RESET, 0, NULL);
}
static void build_skin(void) {
    static char o[900]; join_opts(o, 900, "", skin_theme_count(), skin_theme_name, skin_theme_label);
    static const Def d1[] = { B_("skin_enabled", "Custom title bars on program windows"), B_("skin_hide_native", "Disable Wine's own title bar (the custom bar replaces it)"),
        S_("skin_height", "Title bar height (with Wine title bar removed)", 22, 56), C_("skin_title_align", "Title text alignment", "auto|left|center|right"), C_("skin_icon_pos", "Window icon position", "auto|left|title=Next to title|off"), C_("skin_buttons_side", "Buttons side", "auto|left|right"), K_("skin_color", "Title bar colour (empty = theme)"),
        B_("skin_btn_pin", "Pin (always on top) button"), B_("skin_btn_snap", "Snap button"), B_("skin_btn_shade", "Roll-up button"),
        B_("skin_click_anim", "Click animation on buttons"), H_("Button hover colours (empty = theme)"),
        K_("skin_btn_hover.min", "Minimise hover"), K_("skin_btn_hover.max", "Maximise / restore hover"), K_("skin_btn_hover.close", "Close hover"),
        K_("skin_btn_hover.pin", "Pin hover"), K_("skin_btn_hover.snap", "Snap hover"), K_("skin_btn_hover.shade", "Roll-up hover"),
        H_("Your own button icons (png / ico)"), P_("skin_icons.min", "Minimise", "img"), P_("skin_icons.max", "Maximise", "img"), P_("skin_icons.restore", "Restore", "img"), P_("skin_icons.close", "Close", "img"), P_("skin_icons.pin", "Pin", "img"), P_("skin_icons.snap", "Snap", "img"), P_("skin_icons.shade", "Roll-up", "img"),
        H_("Create and save a title bar theme"), T_("skin_draft.name", "Theme name"), K_("skin_draft.on", "Active bar colour"), K_("skin_draft.off", "Inactive bar colour"), K_("skin_draft.ton", "Active text colour"), K_("skin_draft.toff", "Inactive text colour"),
        C_("skin_draft.grad", "Gradient", "no|yes"), C_("skin_draft.shape", "Corners", "square|round"), C_("skin_draft.left", "Buttons side", "right|left"), C_("skin_draft.center", "Title alignment", "left|center"),
        X_(A_SKIN_SAVE, "Save as a new theme and use it"), X_(A_SKIN_DEL, "Delete the selected custom theme") };
    add(D_CHOICE, "skin_theme", "Title bar style", 0, 0, o); add_defs(d1, (int)(sizeof d1 / sizeof d1[0]));
}
static void build_tray(void) { add(D_INFO, "", "Programs shown as small launcher icons in the taskbar's tray area. Click to open, right-click to remove. You can also use 'Add to tray' in any program's menu.", 0, 0, NULL); add_list("tray_apps", "Add a program to the tray...", "exe"); }
static void build_startbtn(void) {
    static const Def d[] = { C_("start_style", "Start button style", "ninebox=Nine boxes|winlogo=Window logo|classic=Classic Start (Windows 98)|xp=Windows XP|orb=Windows 7 orb"),
        S_("start_icon_scale", "Icon size %", 40, 200), S_("start_btn_w", "Button width (0 = automatic, px)", 0, 300), S_("start_btn_h", "Button height (0 = automatic, px)", 0, 200), K_("start_color", "Icon colour (empty = theme)") };
    add_defs(d, (int)(sizeof d / sizeof d[0])); add(D_HEAD, "", "Your own Start button icon", 0, 0, NULL);
    add(D_INFO, "", "Pick any .png / .ico / .jpg image below. Use the colour box to recolour it, the switch to hide the button.", 0, 0, NULL); add(D_ELEM, el_id(E_START), el_label(E_START), E_START, 0, NULL);
}
static void build_desktop(void) {
    static const Def d[] = { B_("full_desktop", "Full desktop mode (WinShell draws the desktop)"), P_("wallpaper", "Wallpaper image", "img"),
        C_("wallpaper_mode", "Wallpaper mode", "fill=Fill|fit=Fit|stretch=Stretch|center=Center|tile=Tile"), K_("desktop_color", "Background colour"),
        B_("desktop_lock", "Lock all shortcuts (no moving or resizing)"), B_("desktop_grid", "Snap to grid"), S_("grid_size", "Grid / default item size (px)", 48, 200), S_("desktop_icon_size", "Maximum icon size (px)", 24, 128), B_("desktop_labels", "Show names under icons"), S_("widget_alpha", "Widget / tile visibility % (0 = invisible)", 0, 100),
        H_("Shortcuts sync"), B_("sync_desktop_to_start", "Wine / desktop shortcuts automatically appear in the Start app list and tiles"), B_("sync_start_to_desktop", "Start apps and tiles automatically appear on the desktop"),
        X_(A_DESK_SYNC, "Sync now"), H_("Add to the desktop"), X_(A_DESK_ADD, "Add a shortcut..."), X_(A_DESK_TILE, "Add an app tile..."), X_(A_DESK_CLOCK, "Add clock widget"), X_(A_DESK_CAL, "Add calendar widget"), X_(A_DESK_SYS, "Add system widget"), X_(A_DESK_IMAGE, "Add an image tile..."),
        X_(A_DESK_ARRANGE, "Auto-arrange items"), X_(A_DESK_CLEAR, "Remove all desktop items") };
    add(D_INFO, "", "Right-click the desktop for the same options. Drag items to move them, drag the corner square to resize, right-click an item to lock it or change its size and icon.", 0, 0, NULL); add_defs(d, (int)(sizeof d / sizeof d[0]));
}
static void build_game(void) {
    static const Def d[] = { C_("game_theme", "Game mode interface", "ps5=PlayStation 5|steam=Steam Big Picture|switch=Nintendo Switch|xbox=Xbox|retro=Retro list"), T_("game_title", "Title text"),
        B_("game_return", "Come back to game mode when a game closes"), B_("game_autostart", "Open game mode when WinShell starts"), X_(A_GAME_OPEN, "Open game mode now"), X_(A_GAME_ADD, "Add a game...") };
    add(D_INFO, "", "Arrow keys / controller D-pad move, Enter / A play, O / Y opens this page, Esc / B exits. Each game can have its own icon, background picture, description and colour.", 0, 0, NULL); add_defs(d, (int)(sizeof d / sizeof d[0]));
    JNode *g = cfg_node("games"); int i = 0;
    for (JNode *c = g ? g->child : NULL; c; c = c->next, i++) { char k[64]; JNode *nm = json_get(c, "name"); add(D_HEAD, "", nm && nm->s && nm->s[0] ? nm->s : "Game", 0, 0, NULL);
        snprintf(k, 64, "games.%d.name", i); add(D_STR, k, "Name", 0, 0, NULL); snprintf(k, 64, "games.%d.path", i); add(D_FILE, k, "Program to start", 0, 0, "exe");
        snprintf(k, 64, "games.%d.icon", i); add(D_FILE, k, "Icon / cover image", 0, 0, "img"); snprintf(k, 64, "games.%d.bg", i); add(D_FILE, k, "Background image", 0, 0, "img");
        snprintf(k, 64, "games.%d.desc", i); add(D_STR, k, "Description", 0, 0, NULL); snprintf(k, 64, "games.%d.color", i); add(D_COLOR, k, "Tile colour (when no image)", 0, 0, NULL);
        snprintf(k, 64, "games.%d.args", i); add(D_STR, k, "Start arguments", 0, 0, NULL); add(D_BUTTON, "", "Remove this game", A_GAME_DEL + i, 0, NULL); }
}
static const char *BUILTIN_APPS[] = { "winshell:settings", "winshell:game", "winshell:desktop", "winshell:layout" };
static void build_fake(void) {
    add(D_INFO, "", "Fake apps are icon-only launchers: pick an image, give it a name, and optionally a program or action it opens. WinShell's own Settings, Game mode, desktop switch and layout editor are available as apps too. Add any of them to the Start list, tiles, taskbar, desktop or tray.", 0, 0, NULL);
    add(D_BUTTON, "", "Add a fake app from an image...", A_FAKE_ADD, 0, NULL);
    JNode *a = cfg_node("fake_apps"); int i = 0;
    for (JNode *c = a ? a->child : NULL; c; c = c->next, i++) { char k[64]; JNode *nm = json_get(c, "name"); add(D_HEAD, "", nm && nm->s ? nm->s : "App", 0, 0, NULL);
        snprintf(k, 64, "fake_apps.%d.name", i); add(D_STR, k, "Name", 0, 0, NULL); snprintf(k, 64, "fake_apps.%d.icon", i); add(D_FILE, k, "Icon image", 0, 0, "img");
        snprintf(k, 64, "fake_apps.%d.target", i); add(D_STR, k, "Opens (program path or winshell:settings / winshell:game / winshell:desktop)", 0, 0, NULL);
        add(D_BUTTON, "", "Add to Start list and tiles", A_FAKE_BASE + i * 10, 0, NULL); add(D_BUTTON, "", "Add to taskbar", A_FAKE_BASE + i * 10 + 1, 0, NULL); add(D_BUTTON, "", "Add to desktop", A_FAKE_BASE + i * 10 + 2, 0, NULL);
        add(D_BUTTON, "", "Add to tray", A_FAKE_BASE + i * 10 + 3, 0, NULL); add(D_BUTTON, "", "Remove this fake app", A_FAKE_BASE + i * 10 + 4, 0, NULL); }
    add(D_HEAD, "", "Built-in WinShell apps", 0, 0, NULL);
    for (int b = 0; b < 4; b++) { wchar_t w[MAX_PATH], nm[128]; w_from_utf8_buf(BUILTIN_APPS[b], w, MAX_PATH); display_name(w, nm, 128); char *n8 = utf8_from_w(nm); add(D_INFO, "", n8, 0, 0, NULL); free(n8);
        add(D_BUTTON, "", "Add to Start list and tiles", A_BUILTIN_BASE + b * 10, 0, NULL); add(D_BUTTON, "", "Add to taskbar", A_BUILTIN_BASE + b * 10 + 1, 0, NULL); add(D_BUTTON, "", "Add to desktop", A_BUILTIN_BASE + b * 10 + 2, 0, NULL); add(D_BUTTON, "", "Add to tray", A_BUILTIN_BASE + b * 10 + 3, 0, NULL); }
}
static void build_pinned(void) { add_list("pinned", "Add a program to the taskbar...", "exe"); }
static void build_pins(void) { add_list("start_pins", "Pin a program to Start...", "exe"); add_list("start_apps", "Add a program to the app list...", "exe"); }
static void build_strip_ex(void) { add_list("strip_exclude", "Add a program...", "exe"); }
static void build_skin_ex(void) { add_list("skin_exclude", "Add a program...", "exe"); }

static const char DIRS[] = "auto=Automatic|left=Slide from left|right=Slide from right|up=Slide from top|down=Slide from bottom";
static const Def D_ANIM[] = {
  B_("animations", "Animations (master switch)"),
  H_("Taskbar"), B_("anim_bar_on", "Animate the taskbar"), C_("anim_bar_dir", "Direction", DIRS), S_("anim_bar_ms", "Speed: duration (ms), lower is faster", 40, 1200),
  H_("Start panel"), B_("anim_start_on", "Animate the Start panel"), C_("anim_start_dir", "Direction", DIRS), S_("anim_start_ms", "Speed: duration (ms)", 40, 1200),
  H_("Programs: open"), B_("anim_app_on", "Animate windows opening"), C_("anim_app_dir", "Direction", "left=Slide from left|right=Slide from right|up=Slide from top|down=Slide from bottom"), S_("anim_app_ms", "Speed: duration (ms)", 40, 1200),
  H_("Programs: close"), B_("anim_app_close_on", "Animate windows closing (from the taskbar / custom title bar)"), C_("anim_app_close_dir", "Direction", "left=Slide to left|right=Slide to right|up=Slide to top|down=Slide to bottom"), S_("anim_app_close_ms", "Speed: duration (ms)", 40, 1200),
};
static const Def D_GLASS[] = { B_("glass", "Glass effect (translucent bars and menus)"), S_("glass_opacity", "Glass opacity %", 30, 100), S_("glass_gloss", "Gloss (top shine)", 0, 100),
  B_("round_corners", "Rounded corners"), S_("corner_radius", "Corner roundness (px)", 0, 30) };
static const Def D_ALPHA[] = { I_("Fade icons and text. 100 = fully visible."), H_("Taskbar"), S_("bar_icon_alpha", "Icons visibility %", 10, 100), S_("bar_text_alpha", "Text visibility %", 10, 100),
  H_("Start panel"), S_("start_icon_alpha", "Icons visibility %", 10, 100), S_("start_text_alpha", "Text visibility %", 10, 100) };
static const Def D_SHAPES[] = { C_("icon_shape", "Icon shape (taskbar, Start, desktop)", "none|circle|square|rect|rounded"), K_("icon_fill_color", "Shape fill colour (empty = none / automatic)"),
  B_("icon_letter", "Apps without an icon show their first letter on a colour-filled shape"), H_("Pill style"), B_("ui_pills", "Buttons, switches and labels become colour-filled shapes sized to their text"), X_(A_PILL_SHUFFLE, "Random colour shuffle"), B_("icon_border", "Border around the icon shape"),
  S_("icon_border_width", "Border width (px)", 1, 6), K_("icon_border_color", "Border colour (empty = accent)") };
static const Def D_TASKBAR[] = {
  C_("position", "Position", "bottom=Bottom|top=Top|left=Left (vertical)|right=Right (vertical)"), C_("alignment", "Alignment", "left=Start / left|center=Centre|right=End / right"),
  S_("height", "Thickness (height, or width when vertical)", 24, 120), S_("icon_size", "Icon size", 12, 64), B_("show_titles", "Show window titles (horizontal bar)"),
  B_("bar_mirror", "Mirror the bar (Start at the opposite end)"), B_("bar_float", "Floating bar (detached from the screen edge)"), S_("bar_margin", "Floating: side margin (px)", 0, 300),
  S_("bar_gap", "Floating: gap from the edge (px)", 0, 60), C_("bar_style", "Bar style", "flat|raised"), K_("bar_color", "Bar colour (empty = theme)"),
  B_("reserve_work_area", "Maximised windows stop at the bar"), B_("bar_autohide", "Auto-hide the bar"), S_("reveal_seconds", "Auto-hide: stay visible (s)", 2, 20),
  B_("hide_wine_taskbar", "Hide Wine's own taskbar"), B_("tray_icons", "Show tray icons"), B_("show_separator", "Separator between pinned and open apps"), B_("show_preview_button", "Window list button") };
static const Def D_SEARCH[] = {
  B_("search_box", "Search box on the taskbar (Cortana style)"), B_("start_search_in_bar", "Move the Start panel's search into the taskbar box"),
  S_("search_w", "Search box width (px)", 80, 700), T_("search_text", "Placeholder text") };
static const Def D_CLOCK[] = {
  T_("clock_format", "Clock format (%I %H %M %p %d %m %Y %a %b)"), C_("clock_timezone", "Time zone", "india=India (IST)|system=This device|custom=Custom offset"),
  S_("clock_utc_offset", "Custom UTC offset (minutes)", -840, 840), S_("clock_w", "Clock width (px)", 50, 240), S_("preview_w", "Window preview size (px)", 140, 460),
  S_("desk_w", "Show desktop button width (px)", 2, 80), S_("tray_cell", "Tray icon cell size (px)", 16, 48) };
static const Def D_STRIP[] = {
  I_("Auto-hide works together with the reveal bar: while the taskbar is hidden, hover or click the strip to bring it back."), B_("bar_autohide", "Auto-hide the taskbar"), S_("reveal_seconds", "Taskbar stays visible after reveal (s)", 2, 20),
  B_("strip_enabled", "Reveal bar (hover the screen edge to bring the taskbar back)"), C_("strip_pos", "Position along the edge", "start|center|end"),
  S_("strip_width", "Length (% of the edge)", 10, 100), S_("strip_height", "Thickness (px)", 2, 40), S_("strip_opacity", "Opacity % (0 = invisible)", 0, 100),
  K_("strip_color", "Colour (empty = accent)"), S_("strip_seconds", "Bar stays (s)", 1, 20), B_("strip_maximized", "Active over maximised windows too") };
static const Def D_START[] = {
  C_("start_size", "Panel size (5 levels + full screen)", "tiny=Level 1 (tiny)|small=Level 2 (small)|normal=Level 3 (normal)|large=Level 4 (large)|huge=Level 5 (huge)|full=Full screen"),
  B_("start_rounded", "Rounded corners"), C_("start_rail_pos", "Power / settings column position", "left|right|top|bottom"),
  C_("start_tiles_side", "Pinned tiles / links side", "right|left|top=Above the app list (up / down)|bottom=Below the app list (up / down)"), S_("start_stack_split", "Up / down layout: tiles window height %", 20, 80), C_("start_pane", "Right pane shows", "tiles=Pinned tiles|links=Links (documents, pictures...)"),
  B_("start_tiles_collapsed", "Hide the right pane"), B_("start_autoclose", "Close the Start panel when clicking anywhere else"),
  C_("list_view", "App list view", "list=List|list2=2 columns|grid2=Grid 2|grid3=Grid 3|grid4=Grid 4|grid5=Grid 5|tiles=Tiles (8 sizes)"), C_("pane_view", "Pinned items view", "tiles=Tiles (8 sizes)|grid2=Grid 2|grid3=Grid 3|grid4=Grid 4|grid5=Grid 5|list2=2 columns"), B_("start_list_collapsed", "Collapse the app list"),
  B_("start_programs_mode", "Programs button: group installed programs in folders"), B_("start_show_recent", "Show recently used"), B_("start_show_most_used", "Show most used"),
  K_("vbar_color", "Power column colour (empty = theme)") };
static const Def D_SIZES[] = {
  S_("tile_level", "Pinned tiles size (8 levels)", 1, 8), S_("tile_w_adj", "Tile width %", 50, 200), S_("tile_h_adj", "Tile height %", 50, 200),
  S_("list_w", "App list width (px)", 160, 520), S_("list_row_h", "App list row height (px)", 24, 72), S_("rail_w", "Power column width (px)", 32, 96),
  S_("avatar_size", "Profile picture size (px)", 20, 96), S_("search_h", "Start search box height (px)", 22, 56), S_("pane_w", "Right pane width (px)", 140, 400),
  H_("Whole Start panel (0 = automatic)"), S_("start_width", "Width (px)", 0, 1800), S_("start_height", "Height (px)", 0, 1400), S_("start_off_x", "Move left / right (px)", -800, 800), S_("start_off_y", "Move up / down (px)", -800, 800),
  H_("Windows inside the panel (0 = automatic)"), S_("tiles_w", "Pinned tiles window width (px)", 0, 900), S_("tiles_h", "Pinned tiles window height (px)", 0, 1200), S_("list_h", "App list window height (px)", 0, 1200), S_("rail_h", "Power column length %", 20, 100) };
static const Def D_TILES[] = { C_("tile_color_mode", "Tile colours", "multi|accent|icon|custom"), B_("tile_shuffle", "Shuffle tiles automatically"), S_("tile_shuffle_seconds", "Shuffle every (s)", 3, 30),
  X_(A_SHUFFLE_TILES, "Random: shuffle sizes and colours of pinned tiles now") };
static const Def D_SYSTEM[] = { B_("autostart", "Start WinShell with Windows"), T_("user_name", "User name"), P_("user_photo", "Profile picture", "img"),
  X_(A_OPEN_CFG, "Open config.json in Notepad"), H_("Backup and restore"),
  X_(A_BACKUP_SAVE, "Back up settings, images, icons and shortcuts to a file..."), X_(A_BACKUP_RESTORE, "Restore from a backup file..."), X_(A_RELOAD, "Reload settings from file"), X_(A_RESTART, "Restart WinShell") };

#define DEFS(a) a, (int)(sizeof a / sizeof a[0]), NULL
static const Page PAGES[] = {
  { "Appearance", "OS theme", NULL, 0, build_os },
  { "Appearance", "Personalization", NULL, 0, build_personal },
  { "Appearance", "Glass and corners", DEFS(D_GLASS) },
  { "Appearance", "Icon shapes", DEFS(D_SHAPES) },
  { "Appearance", "Transparency", DEFS(D_ALPHA) },
  { "Appearance", "Animations", DEFS(D_ANIM) },
  { "Taskbar", "Taskbar", DEFS(D_TASKBAR) },
  { "Taskbar", "Start button", NULL, 0, build_startbtn },
  { "Taskbar", "Search box", DEFS(D_SEARCH) },
  { "Taskbar", "Clock and buttons", DEFS(D_CLOCK) },
  { "Taskbar", "Layout editor", NULL, 0, build_layout_editor },
  { "Taskbar", "Reveal bar", DEFS(D_STRIP) },
  { "Taskbar", "Reveal bar exceptions", NULL, 0, build_strip_ex },
  { "Taskbar", "Taskbar apps", NULL, 0, build_pinned },
  { "Taskbar", "Tray apps", NULL, 0, build_tray },
  { "Apps", "Fake and built-in apps", NULL, 0, build_fake },
  { "Start panel", "Start panel style", NULL, 0, build_start_theme },
  { "Start panel", "Start panel", DEFS(D_START) },
  { "Start panel", "Start panel sizes", DEFS(D_SIZES) },
  { "Start panel", "Right pane links", NULL, 0, build_pane },
  { "Start panel", "Start elements", NULL, 0, build_start_elems },
  { "Start panel", "Tiles and apps", NULL, 0, build_pins },
  { "Start panel", "Tile colours", DEFS(D_TILES) },
  { "Desktop and games", "Full desktop", NULL, 0, build_desktop },
  { "Desktop and games", "Game mode", NULL, 0, build_game },
  { "Window title bar", "Title bar", NULL, 0, build_skin },
  { "Window title bar", "Title bar exceptions", NULL, 0, build_skin_ex },
  { "Wine controls", "Wine controls", NULL, 0, build_sys },
  { "System", "Startup and user", DEFS(D_SYSTEM) },
};
#define NPAGES ((int)(sizeof PAGES / sizeof PAGES[0]))

static int seg_count(const Item *it);

/* ---------------------------------------------------------------- geometry */
static RECT RC(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }
static int cap_h(void) { return px(34); }
static int nav_w(void) { return px(250); }
static void client_size(int *w, int *h) { RECT rc = { 0, 0, 0, 0 }; GetClientRect(g_hwnd, &rc); *w = rc.right; *h = rc.bottom; }
static int content_left(void) { return nav_w() + px(32); }
static int content_right(void) { int w, h; client_size(&w, &h); return w - px(36); }
static int content_top(void) { return cap_h() + px(64); }
static int view_h(void) { int w, h; client_size(&w, &h); return h - content_top(); }

static int item_height(const Item *it, HDC dc) {
    switch (it->type) {
    case D_HEAD: return px(46);
    case D_INFO: {
        wchar_t w[400]; w_from_utf8_buf(it->label, w, 400);
        RECT r = RC(0, 0, content_right() - content_left(), 0);
        HGDIOBJ o = SelectObject(dc, g_font_small); DrawTextW(dc, w, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX); SelectObject(dc, o);
        return r.bottom + px(16); }
    case D_SLIDER: case D_FSLIDER: return px(62);
    case D_BUTTON: case D_LISTADD: return px(50);
    case D_LISTROW: return px(34);
    case D_ELEM: return px(54);
    case D_PLINK: return px(50);
    case D_SYSCLR: return px(40);
    default: return px(46);
    }
}

static void layout_items(void) {
    HDC dc = GetDC(g_hwnd); int y = 0;
    for (int i = 0; i < g_nitems; i++) { g_items[i].h = item_height(&g_items[i], dc); g_items[i].y = y; y += g_items[i].h; }
    ReleaseDC(g_hwnd, dc); g_content_h = y + px(24);
    int mx = g_content_h - view_h(); if (mx < 0) mx = 0; if (g_scroll > mx) g_scroll = mx; if (g_scroll < 0) g_scroll = 0;
}

static void build_page(void) {
    g_nitems = 0; const Page *p = &PAGES[g_page];
    if (p->build) p->build(); else add_defs(p->defs, p->n);
    layout_items();
}

/* sub-controls of an item, in client coordinates */
static int parts_of(const Item *it, RECT *p) {
    int l = content_left(), r = content_right(), top = content_top() + it->y - g_scroll, h = it->h, ch = px(30);
    int cy = top + (h - ch) / 2; memset(p, 0, sizeof(RECT) * 5);
    switch (it->type) {
    case D_BOOL: p[0] = RC(r - px(46), top + (h - px(22)) / 2, r, top + (h - px(22)) / 2 + px(22)); return 1;
    case D_CHOICE: { int sn = seg_count(it); int w = sn ? sn * px(100) : px(250); if (w > px(520)) w = px(520); p[0] = RC(r - w, cy, r, cy + ch); return 1; }
    case D_STR: p[0] = RC(r - px(300), cy, r, cy + ch); return 1;
    case D_COLOR: case D_SYSCLR: p[1] = RC(r - px(28), cy, r, cy + ch); p[0] = RC(p[1].left - px(8) - px(150), cy, p[1].left - px(8), cy + ch); return 2;
    case D_FILE: p[1] = RC(r - px(28), cy, r, cy + ch); p[0] = RC(p[1].left - px(8) - px(230), cy, p[1].left - px(8), cy + ch); return 2;
    case D_SLIDER: case D_FSLIDER: p[0] = RC(l, top + h - px(24), r, top + h - px(8)); return 1;
    case D_BUTTON: case D_LISTADD: { int w = r - l; if (w > px(540)) w = px(540); p[0] = RC(l, top + px(5), l + w, top + h - px(5)); return 1; }
    case D_LISTROW: p[0] = RC(r - px(30), top + 2, r, top + h - 2); return 1;
    case D_ELEM: p[0] = RC(r - px(46), top + (h - px(22)) / 2, r, top + (h - px(22)) / 2 + px(22));
        p[1] = RC(p[0].left - px(12) - px(34), cy, p[0].left - px(12), cy + ch); p[2] = RC(p[1].left - px(8) - px(34), cy, p[1].left - px(8), cy + ch); return 3;
    case D_PLINK: p[0] = RC(r - px(46), top + (h - px(22)) / 2, r, top + (h - px(22)) / 2 + px(22));
        p[1] = RC(p[0].left - px(12) - px(34), cy, p[0].left - px(12), cy + ch); p[2] = RC(p[1].left - px(8) - px(34), cy, p[1].left - px(8), cy + ch);
        p[3] = RC(p[2].left - px(12) - px(30), cy, p[2].left - px(12), cy + ch); p[4] = RC(p[3].left - px(4) - px(30), cy, p[3].left - px(4), cy + ch); return 5;
    default: return 0;
    }
}

/* ------------------------------------------------------------------ options */
static int opt_count(const char *opts, int idx, char *val, int vn, wchar_t *lab, int ln) {
    int i = 0; const char *p = opts;
    while (p && *p) {
        const char *e = strchr(p, '|'); size_t len = e ? (size_t)(e - p) : strlen(p);
        if (i == idx) {
            char tmp[200]; lstrcpynA(tmp, p, (int)(len < 199 ? len : 199) + 1);
            char *eq = strchr(tmp, '=');
            if (eq) { *eq = 0; lstrcpynA(val, tmp, vn); w_from_utf8_buf(eq + 1, lab, ln); }
            else { lstrcpynA(val, tmp, vn); w_from_utf8_buf(tmp, lab, ln); }
            return 1;
        }
        i++; p = e ? e + 1 : NULL;
    }
    return 0;
}
static void opt_label(const char *opts, const char *value, wchar_t *out, int n) {
    char v[100]; wchar_t l[100];
    for (int i = 0; opt_count(opts, i, v, 100, l, 100); i++) if (!strcmp(v, value)) { lstrcpynW(out, l, n); return; }
    w_from_utf8_buf(value, out, n);
}

static int seg_count(const Item *it) {
    int n = 0, maxl = 0; char v[100]; wchar_t l[100];
    for (; opt_count(it->opts, n, v, 100, l, 100); n++) { int len = (int)wcslen(l); if (len > maxl) maxl = len; }
    return (n >= 2 && n <= 5 && maxl <= 14) ? n : 0;
}
static int choose(const Item *it, RECT anchor, char *out, int on) {
    static wchar_t labs[100][100]; static int ids[100], fl[100]; static const wchar_t *lp[100]; char v[100]; int n = 0;
    const char *cur = get_str(it->key);
    for (; n < 100 && opt_count(it->opts, n, v, 100, labs[n], 100); n++) { lp[n] = labs[n]; ids[n] = n + 1; fl[n] = !strcmp(v, cur) ? MF_CHECKED : 0; }
    if (!n) return 0;
    POINT pt = { anchor.left, anchor.bottom }; ClientToScreen(g_hwnd, &pt);
    int cmd = app_popup(g_hwnd, lp, ids, fl, n, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y);
    if (cmd < 1 || !opt_count(it->opts, cmd - 1, out, on, labs[0], 100)) return 0;
    return 1;
}

static int pick_color(COLORREF cur, COLORREF *out) {
    static COLORREF custom[16]; static int init;
    if (!init) { for (int i = 0; i < 16; i++) custom[i] = RGB(255, 255, 255); init = 1; }
    CHOOSECOLORW cc; memset(&cc, 0, sizeof cc); cc.lStructSize = sizeof cc; cc.hwndOwner = g_hwnd; cc.rgbResult = cur; cc.lpCustColors = custom; cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    g_menu_depth++; BOOL ok = ChooseColorW(&cc); g_menu_depth--;
    if (ok) *out = cc.rgbResult;
    return ok;
}
static int pick_file(const char *kind, char *out, int n) {
    static const wchar_t IMG[] = L"Images\0*.png;*.ico;*.jpg;*.jpeg;*.bmp;*.gif\0All files\0*.*\0";
    static const wchar_t EXE[] = L"Programs\0*.exe;*.lnk;*.bat\0All files\0*.*\0";
    wchar_t file[MAX_PATH] = L""; OPENFILENAMEW o; memset(&o, 0, sizeof o);
    o.lStructSize = sizeof o; o.hwndOwner = g_hwnd; o.lpstrFile = file; o.nMaxFile = MAX_PATH;
    o.lpstrFilter = kind && !strcmp(kind, "img") ? IMG : EXE; o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    g_menu_depth++; BOOL ok = GetOpenFileNameW(&o); g_menu_depth--;
    if (!ok) return 0;
    char *u = utf8_from_w(file); lstrcpynA(out, u, n); free(u); return 1;
}

/* ----------------------------------------------------------------- painting */
typedef struct { COLORREF bg, nav, text, dim, accent, ctl, ctl_hot, border; } Pal;
static Pal pal(void) {
    Spec sp; app_spec(&sp); Pal p;
    p.bg = sp.bg; p.nav = sp.alt; p.text = sp.text; p.dim = c_mix(sp.text, sp.bg, 0.45); p.accent = app_accent();
    p.ctl = c_mix(sp.bg, sp.text, 0.10); p.ctl_hot = c_mix(sp.bg, sp.text, 0.18); p.border = c_mix(sp.bg, sp.text, 0.32);
    return p;
}

static void draw_switch(HDC dc, RECT r, int on, int hot, const Pal *p) {
    if (cfg_bool("ui_pills")) { g_pill(dc, r, on ? L"ON" : L"OFF", 2, on); return; }
    int h = r.bottom - r.top, w = r.right - r.left;
    HBRUSH b = CreateSolidBrush(on ? p->accent : p->bg); HPEN pen = CreatePen(PS_SOLID, 2, on ? p->accent : (hot ? p->text : p->dim));
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, h, h); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(pen);
    int d = h - px(8), x = on ? r.right - d - px(4) : r.left + px(4);
    HBRUSH k = CreateSolidBrush(on ? RGB(255, 255, 255) : p->dim); ob = SelectObject(dc, k); op = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, x, r.top + px(4), x + d, r.top + px(4) + d); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(k); (void)w;
}
static void draw_box(HDC dc, RECT r, const Pal *p, int hot, const wchar_t *text, int chevron) {
    if (cfg_bool("ui_pills")) { g_pill(dc, r, text[0] ? text : L"-", 2, hot); return; }
    g_fill(dc, r, hot ? p->ctl_hot : p->ctl); g_frame(dc, r, hot ? p->accent : p->border, 1);
    RECT t = RC(r.left + px(10), r.top, r.right - (chevron ? px(26) : px(8)), r.bottom);
    g_text(dc, text, t, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text);
    if (chevron) glyph_draw(G_CHEVRON_D, dc, RC(r.right - px(24), r.top, r.right - px(6), r.bottom), p->dim);
}
static void draw_swatch(HDC dc, RECT r, COLORREF c, int has, const Pal *p, int hot) {
    g_fill(dc, r, hot ? p->ctl_hot : p->ctl); g_frame(dc, r, hot ? p->accent : p->border, 1);
    RECT s = RC(r.left + px(5), r.top + px(5), r.left + px(5) + px(36), r.bottom - px(5));
    if (has) g_fill(dc, s, c); else { g_frame(dc, s, p->dim, 1); }
    wchar_t t[16]; if (has) { char h[8]; to_hex(c, h); w_from_utf8_buf(h, t, 16); } else lstrcpyW(t, L"Default");
    g_text(dc, t, RC(s.right + px(8), r.top, r.right - px(4), r.bottom), DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, has ? p->text : p->dim);
}
static void draw_button(HDC dc, RECT r, const wchar_t *text, const Pal *p, int hot, int accent) {
    if (cfg_bool("ui_pills")) { g_pill(dc, r, text, 0, hot || accent); return; }
    g_fill(dc, r, accent ? (hot ? c_shade(p->accent, 1.12) : p->accent) : (hot ? p->ctl_hot : p->ctl)); g_frame(dc, r, hot ? p->accent : p->border, 1);
    g_text(dc, text, RC(r.left + px(10), r.top, r.right - px(10), r.bottom), DT_VCENTER | DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, accent ? RGB(255, 255, 255) : p->text);
}

static void draw_seg(HDC dc, const Item *it, RECT r, const Pal *p, int sn) {
    const char *cur = get_str(it->key); int w = r.right - r.left; char v[100]; wchar_t l[100];
    for (int k = 0; k < sn; k++) { if (!opt_count(it->opts, k, v, 100, l, 100)) break; RECT s = { r.left + w * k / sn, r.top, r.left + w * (k + 1) / sn, r.bottom }; int on = !strcmp(v, cur);
        if (cfg_bool("ui_pills")) { InflateRect(&s, -2, 0); g_pill(dc, s, l, 1, on); continue; }
        g_fill(dc, s, on ? p->accent : p->ctl); g_frame(dc, s, on ? p->accent : p->border, 1);
        g_text(dc, l, s, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, on ? RGB(255, 255, 255) : p->text); }
}

static void draw_item(HDC dc, int idx, const Pal *p, int L, int Rr) {
    Item *it = &g_items[idx]; RECT pr[5]; int np = parts_of(it, pr); int top = content_top() + it->y - g_scroll;
    wchar_t lab[200], tmp[260]; w_from_utf8_buf(it->label, lab, 200);
    RECT lr = RC(L, top, (np > 0 && (it->type == D_BOOL || it->type == D_CHOICE || it->type == D_COLOR || it->type == D_SYSCLR || it->type == D_STR || it->type == D_FILE)) ? pr[0].left - px(12) : Rr - px(160), top + it->h); int hov = g_hover == idx;
    #define HOT(n) (hov && g_hover_part == (n))
    switch (it->type) {
    case D_HEAD: { HGDIOBJ o = SelectObject(dc, g_font_big); g_text(dc, lab, RC(L, top + px(14), Rr, top + it->h), DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, p->accent); SelectObject(dc, o); break; }
    case D_INFO: { HGDIOBJ o = SelectObject(dc, g_font_small); g_text(dc, lab, RC(L, top, Rr, top + it->h), DT_WORDBREAK | DT_NOPREFIX, p->dim); SelectObject(dc, o); break; }
    case D_BOOL: g_text(dc, lab, lr, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text); draw_switch(dc, pr[0], cfg_bool(it->key), HOT(0), p); break;
    case D_SLIDER: case D_FSLIDER: {
        double v = it->type == D_FSLIDER ? cfg_num(it->key) * 100.0 : cfg_num(it->key);
        g_text(dc, lab, RC(L, top + px(6), Rr - px(100), top + px(30)), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text);
        _snwprintf(tmp, 64, L"%d", (int)(v + 0.5)); g_text(dc, tmp, RC(Rr - px(100), top + px(6), Rr, top + px(30)), DT_VCENTER | DT_RIGHT | DT_SINGLELINE, p->dim);
        RECT t = pr[0]; int cy = (t.top + t.bottom) / 2; double f = it->b > it->a ? (v - it->a) / (it->b - it->a) : 0; if (f < 0) f = 0; if (f > 1) f = 1;
        g_fill(dc, RC(t.left, cy - 2, t.right, cy + 2), p->border);
        int kx = t.left + (int)((t.right - t.left) * f); g_fill(dc, RC(t.left, cy - 2, kx, cy + 2), p->accent);
        HBRUSH b = CreateSolidBrush(hov || g_drag == idx ? c_shade(p->accent, 1.15) : p->accent); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, kx - px(8), cy - px(8), kx + px(8), cy + px(8)); SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); break; }
    case D_CHOICE: { g_text(dc, lab, lr, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text); int sn = seg_count(it);
        if (sn) draw_seg(dc, it, pr[0], p, sn); else { opt_label(it->opts, get_str(it->key), tmp, 260); draw_box(dc, pr[0], p, HOT(0), tmp, 1); } break; }
    case D_COLOR: case D_SYSCLR: {
        COLORREF c; int has;
        if (it->type == D_SYSCLR) { c = sys_slot_color(it->a); has = 1; } else { const char *h = get_str(it->key); has = h[0] != 0; c = c_hex(h, 0); }
        g_text(dc, lab, lr, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text); draw_swatch(dc, pr[0], c, has, p, HOT(0));
        if (it->type == D_COLOR ? has : sys_slot_overridden(it->a)) { glyph_draw(G_CLOSE, dc, pr[1], HOT(1) ? p->accent : p->dim); }
        break; }
    case D_STR: { g_text(dc, lab, lr, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text); w_from_utf8_buf(get_str(it->key), tmp, 260); draw_box(dc, pr[0], p, HOT(0), tmp, 0); break; }
    case D_FILE: { g_text(dc, lab, lr, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text); const char *v = get_str(it->key);
        wchar_t w[MAX_PATH]; w_from_utf8_buf(v, w, MAX_PATH); const wchar_t *b = wcsrchr(w, L'\\'); draw_box(dc, pr[0], p, HOT(0), v[0] ? (b ? b + 1 : w) : L"Choose a file...", 0);
        if (v[0]) glyph_draw(G_CLOSE, dc, pr[1], HOT(1) ? p->accent : p->dim); break; }
    case D_BUTTON: case D_LISTADD: draw_button(dc, pr[0], lab, p, HOT(0), it->type == D_BUTTON && (it->a == A_OS_APPLY)); break;
    case D_LISTROW: { w_from_utf8_buf(it->label, tmp, 260); const wchar_t *b = wcsrchr(tmp, L'\\'); g_text(dc, b ? b + 1 : tmp, RC(L + px(8), top, Rr - px(40), top + it->h), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text);
        g_fill(dc, RC(L, top + it->h - 1, Rr, top + it->h), p->border); glyph_draw(G_CLOSE, dc, pr[0], HOT(0) ? p->accent : p->dim); break; }
    case D_ELEM: case D_PLINK: {
        int e = it->a; int on; const char *icon = ""; const char *col = "";
        if (it->type == D_ELEM) { on = el_show(e); icon = el_icon(e); col = el_has_color(e) ? "x" : ""; }
        else { on = it->b; JNode *o = json_get(cfg_node("pane_custom"), it->key); JNode *i = o ? json_get(o, "icon") : NULL, *c = o ? json_get(o, "color") : NULL; icon = i && i->s ? i->s : ""; col = c && c->s ? c->s : ""; }
        g_text(dc, lab, RC(L, top, pr[it->type == D_ELEM ? 2 : 4].left - px(10), top + it->h), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, on ? p->text : p->dim);
        draw_switch(dc, pr[0], on, HOT(0), p);
        g_fill(dc, pr[1], HOT(1) ? p->ctl_hot : p->ctl); g_frame(dc, pr[1], icon[0] ? p->accent : p->border, 1);
        if (icon[0]) { wchar_t w[MAX_PATH]; w_from_utf8_buf(icon, w, MAX_PATH); RECT ir = RC(pr[1].left + 4, pr[1].top + 4, pr[1].right - 4, pr[1].bottom - 4); if (!img_draw(dc, w, ir, 0, 0)) glyph_draw(G_PIC, dc, ir, p->text); }
        else glyph_draw(G_PIC, dc, RC(pr[1].left + 6, pr[1].top + 6, pr[1].right - 6, pr[1].bottom - 6), p->dim);
        g_fill(dc, pr[2], HOT(2) ? p->ctl_hot : p->ctl); g_frame(dc, pr[2], col[0] ? p->accent : p->border, 1);
        { COLORREF cc = it->type == D_ELEM ? el_color(e, p->dim) : c_hex(col, p->dim); g_fill(dc, RC(pr[2].left + 6, pr[2].top + 6, pr[2].right - 6, pr[2].bottom - 6), col[0] ? cc : p->ctl); }
        if (it->type == D_PLINK) { g_fill(dc, pr[3], HOT(3) ? p->ctl_hot : p->ctl); glyph_draw(G_CHEVRON_D, dc, pr[3], p->text); g_fill(dc, pr[4], HOT(4) ? p->ctl_hot : p->ctl); glyph_draw(G_CHEVRON_D, dc, pr[4], p->text); }
        break; }
    }
    #undef HOT
    (void)np;
}

static void draw_nav(HDC dc, const Pal *p, int H) {
    RECT nav = RC(0, cap_h(), nav_w(), H); g_fill(dc, nav, p->nav);
    HRGN rgn = CreateRectRgn(0, cap_h(), nav_w(), H); SelectClipRgn(dc, rgn);
    int y = cap_h() + px(8) - g_nav_scroll; const char *last = "";
    for (int i = 0; i < NPAGES; i++) {
        wchar_t w[100];
        if (strcmp(last, PAGES[i].group)) { last = PAGES[i].group; w_from_utf8_buf(last, w, 100); HGDIOBJ o = SelectObject(dc, g_font_small);
            g_text(dc, w, RC(px(20), y + px(10), nav_w() - px(8), y + px(32)), DT_VCENTER | DT_SINGLELINE, p->dim); SelectObject(dc, o); y += px(34); }
        RECT r = RC(px(8), y, nav_w() - px(8), y + px(36)); w_from_utf8_buf(PAGES[i].name, w, 100);
        if (i == g_page) { g_alpha(dc, r, p->accent, 70); g_fill(dc, RC(r.left, r.top + px(8), r.left + px(3), r.bottom - px(8)), p->accent); }
        else if (i == g_nav_hover) g_alpha(dc, r, p->text, 22);
        if (cfg_bool("ui_pills")) g_pill(dc, RC(r.left + px(12), r.top + px(4), r.right - px(8), r.bottom - px(4)), w, 0, i == g_page);
        else g_text(dc, w, RC(r.left + px(16), r.top, r.right - px(6), r.bottom), DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, p->text);
        y += px(38);
    }
    SelectClipRgn(dc, NULL); DeleteObject(rgn);
}

static void paint_all(HDC dc, int W, int H) {
    Pal p = pal(); g_fill(dc, RC(0, 0, W, H), p.bg); SetBkMode(dc, TRANSPARENT); HGDIOBJ of = SelectObject(dc, g_font);
    draw_nav(dc, &p, H);
    /* content */
    int L = content_left(), Rr = content_right(), ct = content_top();
    HGDIOBJ o = SelectObject(dc, g_font_big); wchar_t t[100]; w_from_utf8_buf(PAGES[g_page].name, t, 100);
    g_text(dc, t, RC(L, cap_h() + px(14), Rr, ct - px(6)), DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, p.text); SelectObject(dc, o);
    HRGN rgn = CreateRectRgn(nav_w(), ct, W, H); SelectClipRgn(dc, rgn);
    for (int i = 0; i < g_nitems; i++) {
        int top = ct + g_items[i].y - g_scroll; if (top + g_items[i].h < ct || top > H) continue;
        draw_item(dc, i, &p, L, Rr);
    }
    int mx = g_content_h - view_h();
    if (mx > 0) { int vh = view_h(), th = vh * vh / g_content_h; if (th < px(30)) th = px(30); int ty = ct + (int)((double)g_scroll / mx * (vh - th));
        g_alpha(dc, RC(W - px(8), ty, W - px(3), ty + th), p.text, 90); }
    SelectClipRgn(dc, NULL); DeleteObject(rgn);
    /* caption */
    g_fill(dc, RC(0, 0, W, cap_h()), c_mix(p.bg, p.text, 0.05));
    HGDIOBJ o2 = SelectObject(dc, g_font); g_text(dc, L"WinShell Settings", RC(px(14), 0, W - px(100), cap_h()), DT_VCENTER | DT_SINGLELINE, p.text); SelectObject(dc, o2);
    RECT cl = RC(W - px(46), 0, W, cap_h()), mn = RC(W - px(92), 0, W - px(46), cap_h());
    if (g_hover == -2) g_fill(dc, cl, RGB(232, 17, 35)); else if (g_hover == -3) g_alpha(dc, mn, p.text, 30);
    glyph_draw(G_MIN, dc, RC(mn.left + px(14), mn.top + px(8), mn.right - px(14), mn.bottom - px(8)), p.text);
    glyph_draw(G_CLOSE, dc, RC(cl.left + px(14), cl.top + px(8), cl.right - px(14), cl.bottom - px(8)), g_hover == -2 ? RGB(255, 255, 255) : p.text);
    g_frame(dc, RC(0, 0, W, H), p.border, 1);
    SelectObject(dc, of);
}

void settings_refresh(void) {
    if (!g_hwnd) return;
    if (g_edit_item < 0) { int keep = g_scroll; build_page(); g_scroll = keep; layout_items(); }
    InvalidateRect(g_hwnd, NULL, FALSE);
}
HWND settings_hwnd(void) { return g_hwnd; }

/* ------------------------------------------------------------- interaction */
static char g_edit_key[64];

static void end_edit(int commit) {
    if (g_edit_item < 0 || !g_edit) return;
    g_edit_item = -1;
    wchar_t w[300]; GetWindowTextW(g_edit, w, 300); ShowWindow(g_edit, SW_HIDE);
    if (commit) { char *u = utf8_from_w(w); set_str(g_edit_key, u); free(u); changed(); } else settings_refresh();
}
static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) { end_edit(1); return 0; }
    if (m == WM_KEYDOWN && w == VK_ESCAPE) { end_edit(0); return 0; }
    if (m == WM_CHAR && (w == 13 || w == 27)) return 0;
    if (m == WM_KILLFOCUS) end_edit(1);
    return CallWindowProcW(g_edit_old, h, m, w, l);
}
static void begin_edit(int idx) {
    Item *it = &g_items[idx]; RECT pr[5]; parts_of(it, pr);
    if (!g_edit) {
        g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 10, 10, g_hwnd, NULL, GetModuleHandleW(NULL), NULL);
        g_edit_old = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)edit_proc);
    }
    SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
    wchar_t w[300]; w_from_utf8_buf(get_str(it->key), w, 300); SetWindowTextW(g_edit, w);
    lstrcpynA(g_edit_key, it->key, 64);
    int fh = px(cfg_int("font_size")) + px(6);
    SetWindowPos(g_edit, HWND_TOP, pr[0].left + px(6), pr[0].top + (pr[0].bottom - pr[0].top - fh) / 2, pr[0].right - pr[0].left - px(12), fh, SWP_SHOWWINDOW);
    g_edit_item = idx; SetFocus(g_edit); SendMessageW(g_edit, 0xB1 /*EM_SETSEL*/, 0, -1);
}

static void notepad_config(void) { wchar_t prm[MAX_PATH + 4]; _snwprintf(prm, MAX_PATH + 4, L"\"%ls\"", g_config_path); app_run(L"notepad.exe", prm, SW_SHOWNORMAL); }

static void array_remove(const char *key, int index) {
    JNode *a = cfg_node(key); if (!a || a->t != J_ARR) return;
    JNode *c = a->child, *prev = NULL; int i = 0;
    while (c) { JNode *nx = c->next; if (i == index) { if (prev) prev->next = nx; else a->child = nx; c->next = NULL; json_free(c); return; } prev = c; c = nx; i++; }
}
static void array_move(const char *key, const char *val, int dir) {
    JNode *a = cfg_node(key); if (!a || a->t != J_ARR) return;
    JNode *c = a->child, *prev = NULL, *pprev = NULL;
    while (c && !(c->s && !strcmp(c->s, val))) { pprev = prev; prev = c; c = c->next; }
    if (!c) return;
    if (dir < 0 && prev) { prev->next = c->next; c->next = prev; if (pprev) pprev->next = c; else a->child = c; }
    else if (dir > 0 && c->next) { JNode *n = c->next; c->next = n->next; n->next = c; if (prev) prev->next = n; else a->child = n; }
}
static JNode *pane_node(const char *id) {
    JNode *o = cfg_node("pane_custom"); if (!o) return NULL;
    JNode *n = json_get(o, id); if (!n) n = json_put(o, id, json_new(J_OBJ)); return n;
}

static void add_path_to(const char *u, int kind) {
    wchar_t w[MAX_PATH]; w_from_utf8_buf(u, w, MAX_PATH); JNode *a;
    if (kind == 0) { a = cfg_node("start_apps"); if (a && !cfg_list_has("start_apps", u)) json_push(a, json_str(u)); a = cfg_node("start_pins"); if (a && !cfg_list_has("start_pins", u)) json_push(a, json_str(u)); }
    else if (kind == 1) bar_pin(w); else if (kind == 2) desktop_add_shortcut(w, "shortcut"); else if (kind == 3) tray_add_app(w);
    changed();
}
static void skin_save_draft(void) {
    const char *nm = get_str("skin_draft.name"); JNode *d = cfg_node("skin_draft"), *c = cfg_node("skin_custom_themes"); if (!nm[0] || !d || !c) return;
    JNode *o = json_new(J_OBJ); static const char *keys[] = { "on", "off", "ton", "toff", "grad", "shape", "left", "center" };
    for (int i = 0; i < 8; i++) { JNode *v = json_get(d, keys[i]); json_put(o, keys[i], json_str(v && v->s ? v->s : "")); }
    json_put(c, nm, o); cfg_set_str("skin_theme", nm);
}
static void do_action(int a) {
    switch (a) {
    case A_OS_APPLY: os_theme_apply(cfg_str("os_theme")); changed(); break;
    case A_SHUFFLE_THEME: shuffle_theme(); changed(); break;
    case A_SHUFFLE_TILES: shuffle_tiles(); changed(); break;
    case A_EDIT_TASKBAR: editor_open(0); break;
    case A_EDIT_START: editor_open(1); break;
    case A_DESK_IMAGE: { char b[320]; if (pick_file("img", b, 320)) { wchar_t w[MAX_PATH]; w_from_utf8_buf(b, w, MAX_PATH); desktop_add_image(w, NULL); changed(); } break; }
    case A_RANDOM_OS: srand(GetTickCount()); os_theme_apply(os_theme_name(rand() % os_theme_count())); changed(); break;
    case A_RANDOM_ALL: shuffle_os(1); changed(); break;
    case A_PILL_SHUFFLE: srand(GetTickCount()); cfg_set_int("pill_seed", 1 + rand() % 9999); cfg_set_bool("ui_pills", 1); changed(); break;
    case A_BACKUP_SAVE: app_backup_save(g_hwnd); break;
    case A_BACKUP_RESTORE: app_backup_restore(g_hwnd); break;
    case A_SKIN_SAVE: skin_save_draft(); changed(); break;
    case A_SKIN_DEL: { JNode *c = cfg_node("skin_custom_themes"); if (c) json_remove(c, cfg_str("skin_theme")); cfg_set_str("skin_theme", "windows10"); changed(); break; }
    case A_FAKE_ADD: { char b[320]; JNode *fa = cfg_node("fake_apps"); if (fa && fa->t == J_ARR && pick_file("img", b, 320)) { JNode *o = json_new(J_OBJ); char id[32]; snprintf(id, 32, "f%lu", (unsigned long)GetTickCount()); const char *bn = strrchr(b, '\\'); char nm[120]; lstrcpynA(nm, bn ? bn + 1 : b, 120); char *dot = strrchr(nm, '.'); if (dot) *dot = 0;
            json_put(o, "id", json_str(id)); json_put(o, "name", json_str(nm)); json_put(o, "icon", json_str(b)); json_put(o, "target", json_str("")); json_put(o, "color", json_str("")); json_push(fa, o); changed(); } break; }
    case A_RESET_ELEMS: el_auto_all(); changed(); break;
    case A_DESK_ADD: case A_DESK_TILE: { char b[320]; if (pick_file("exe", b, 320)) { wchar_t w[MAX_PATH]; w_from_utf8_buf(b, w, MAX_PATH); desktop_add_shortcut(w, a == A_DESK_ADD ? "shortcut" : "tile"); changed(); } break; }
    case A_DESK_CLOCK: desktop_add_widget("clock"); changed(); break; case A_DESK_CAL: desktop_add_widget("calendar"); changed(); break; case A_DESK_SYS: desktop_add_widget("system"); changed(); break;
    case A_DESK_ARRANGE: desktop_arrange(); break; case A_DESK_SYNC: desktop_sync(); changed(); break; case A_DESK_CLEAR: desktop_clear(); changed(); break;
    case A_GAME_OPEN: game_open(); break;
    case A_GAME_ADD: { char b[320]; JNode *g = cfg_node("games"); if (g && g->t == J_ARR && pick_file("exe", b, 320)) { JNode *o = json_new(J_OBJ); wchar_t w[MAX_PATH], nm[128]; w_from_utf8_buf(b, w, MAX_PATH); display_name(w, nm, 128); char *nu = utf8_from_w(nm);
            json_put(o, "name", json_str(nu)); free(nu); json_put(o, "path", json_str(b)); json_put(o, "icon", json_str("")); json_put(o, "bg", json_str("")); json_put(o, "desc", json_str("")); json_put(o, "color", json_str("")); json_put(o, "args", json_str("")); json_push(g, o); changed(); } break; }
    default:
        if (a >= A_BUILTIN_BASE) { int b = (a - A_BUILTIN_BASE) / 10, kind = (a - A_BUILTIN_BASE) % 10; if (b < 4) add_path_to(BUILTIN_APPS[b], kind); }
        else if (a >= A_FAKE_BASE) { int i = (a - A_FAKE_BASE) / 10, kind = (a - A_FAKE_BASE) % 10; JNode *fa = cfg_node("fake_apps"), *f = fa ? fa->child : NULL; for (int k = 0; f && k < i; k++) f = f->next;
            if (f) { JNode *id = json_get(f, "id"); if (kind == 4) { array_remove("fake_apps", i); changed(); } else if (id && id->s) { char p[80]; snprintf(p, 80, "fake:%s", id->s); add_path_to(p, kind); } } }
        else if (a >= A_GAME_DEL) { array_remove("games", a - A_GAME_DEL); changed(); } break;
    case A_OPEN_CFG: notepad_config(); break;
    case A_RELOAD: app_reload(); settings_refresh(); break;
    case A_RESTART: app_restart(); break;
    case A_SYS_RESET: { JNode *o = cfg_node("ctrl_colors"); if (o) { json_free(o->child); o->child = NULL; } changed(); break; }
    case A_PANE_RESET: { const char *d[] = { "documents", "pictures", "music", "computer", "controlpanel", "run" }; cfg_list_set("pane_items", d, 6);
        JNode *o = cfg_node("pane_custom"); if (o) { json_free(o->child); o->child = NULL; } changed(); break; }
    }
}

static int hit_item(int x, int y, int *part) {
    int ct = content_top(); *part = -1;
    if (x < nav_w() || y < ct) return -1;
    for (int i = 0; i < g_nitems; i++) {
        Item *it = &g_items[i]; int top = ct + it->y - g_scroll;
        if (y < top || y >= top + it->h) continue;
        RECT pr[5]; int n = parts_of(it, pr);
        for (int k = 0; k < n; k++) if (pr[k].right > pr[k].left && in_rect(&pr[k], x, y)) { *part = k; return i; }
        if (it->type == D_BOOL) *part = 0;
        return i;
    }
    return -1;
}

static void slider_set(int idx, int x) {
    Item *it = &g_items[idx]; RECT pr[5]; parts_of(it, pr);
    double f = (double)(x - pr[0].left) / (double)(pr[0].right - pr[0].left); if (f < 0) f = 0; if (f > 1) f = 1;
    double v = it->a + f * (it->b - it->a);
    if (it->type == D_FSLIDER) json_put(g_cfg, it->key, json_num((int)(v + 0.5) / 100.0, 0)); else cfg_set_int(it->key, (int)(v + 0.5));
}

static void activate(int idx, int part, int x, int right) {
    Item *it = &g_items[idx]; char buf[320]; COLORREF c;
    switch (it->type) {
    case D_BOOL: cfg_set_bool(it->key, !cfg_bool(it->key)); changed(); break;
    case D_SLIDER: case D_FSLIDER: g_drag = idx; SetCapture(g_hwnd); slider_set(idx, x); InvalidateRect(g_hwnd, NULL, FALSE); break;
    case D_CHOICE: { RECT pr[5]; parts_of(it, pr); int sn = seg_count(it);
        if (sn) { int k = (x - pr[0].left) * sn / (pr[0].right - pr[0].left); if (k < 0) k = 0; if (k >= sn) k = sn - 1; char v[100]; wchar_t l[100]; if (opt_count(it->opts, k, v, 100, l, 100)) { set_str(it->key, v); changed(); } break; }
        if (choose(it, pr[0], buf, 320)) { set_str(it->key, buf); changed(); } break; }
    case D_COLOR: if (part == 1 || right) { set_str(it->key, ""); changed(); }
        else { const char *h = get_str(it->key); c = h[0] ? c_hex(h, 0) : app_accent(); if (pick_color(c, &c)) { char hx[8]; to_hex(c, hx); set_str(it->key, hx); changed(); } } break;
    case D_SYSCLR: if (part == 1 || right) { sys_slot_set(it->a, ""); changed(); }
        else { c = sys_slot_color(it->a); if (pick_color(c, &c)) { char hx[8]; to_hex(c, hx); sys_slot_set(it->a, hx); cfg_set_bool("ctrl_enabled", 1); changed(); } } break;
    case D_STR: begin_edit(idx); break;
    case D_FILE: if (part == 1 || right) { set_str(it->key, ""); changed(); } else if (pick_file(it->opts, buf, 320)) { set_str(it->key, buf); changed(); } break;
    case D_BUTTON: do_action(it->a); break;
    case D_LISTROW: array_remove(it->key, it->a); changed(); break;
    case D_LISTADD: if (pick_file("exe", buf, 320)) { JNode *a = cfg_node(it->key); if (a && a->t == J_ARR && !cfg_list_has(it->key, buf)) json_push(a, json_str(buf)); changed(); } break;
    case D_ELEM:
        if (part == 0) { el_set_show(it->a, !el_show(it->a)); changed(); }
        else if (part == 1) { if (right) el_set_icon(it->a, ""); else if (pick_file("img", buf, 320)) el_set_icon(it->a, buf); else break; changed(); }
        else if (part == 2) { if (right) el_set_color(it->a, ""); else { c = el_color(it->a, app_accent()); if (pick_color(c, &c)) { char hx[8]; to_hex(c, hx); el_set_color(it->a, hx); } else break; } changed(); }
        break;
    case D_PLINK: {
        const char *id = it->key; JNode *n;
        if (part == 0) { if (it->b) { JNode *arr = cfg_node("pane_items"); int i = 0; for (JNode *k = arr ? arr->child : NULL; k; k = k->next, i++) if (k->s && !strcmp(k->s, id)) { array_remove("pane_items", i); break; } }
            else { JNode *arr = cfg_node("pane_items"); if (arr) json_push(arr, json_str(id)); } changed(); }
        else if (part == 1) { n = pane_node(id); if (!n) break; if (right) json_put(n, "icon", json_str("")); else if (pick_file("img", buf, 320)) json_put(n, "icon", json_str(buf)); else break; changed(); }
        else if (part == 2) { n = pane_node(id); if (!n) break; if (right) json_put(n, "color", json_str("")); else { JNode *cc = json_get(n, "color"); c = cc && cc->s && cc->s[0] ? c_hex(cc->s, 0) : app_accent(); if (pick_color(c, &c)) { char hx[8]; to_hex(c, hx); json_put(n, "color", json_str(hx)); } else break; } changed(); }
        else if (part == 3 || part == 4) { array_move("pane_items", id, part == 3 ? -1 : 1); changed(); }
        break; }
    }
}

static int nav_hit(int x, int y) {
    if (x >= nav_w() || y < cap_h()) return -1;
    int yy = cap_h() + px(8) - g_nav_scroll; const char *last = "";
    for (int i = 0; i < NPAGES; i++) {
        if (strcmp(last, PAGES[i].group)) { last = PAGES[i].group; yy += px(34); }
        if (y >= yy && y < yy + px(36)) return i;
        yy += px(38);
    }
    return -1;
}

static LRESULT CALLBACK settings_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int hd; LRESULT mr;
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(h, &ps); int W, H; client_size(&W, &H);
        HDC mem = CreateCompatibleDC(hdc); HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H); HGDIOBJ old = SelectObject(mem, bmp);
        paint_all(mem, W, H); BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_NCCALCSIZE: if (w) return 0; break;
    case WM_GETMINMAXINFO: { MINMAXINFO *mi = (MINMAXINFO *)l; mi->ptMinTrackSize.x = px(760); mi->ptMinTrackSize.y = px(500); return 0; }
    case WM_NCHITTEST: {
        POINT pt = { (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &pt); int W, H; client_size(&W, &H); int b = px(6);
        int left = pt.x < b, rightb = pt.x >= W - b, topb = pt.y < b, bot = pt.y >= H - b;
        if (topb && left) return HTTOPLEFT; if (topb && rightb) return HTTOPRIGHT; if (bot && left) return HTBOTTOMLEFT; if (bot && rightb) return HTBOTTOMRIGHT;
        if (left) return HTLEFT; if (rightb) return HTRIGHT; if (topb) return HTTOP; if (bot) return HTBOTTOM;
        if (pt.y < cap_h() && pt.x < W - px(92)) return HTCAPTION;
        return HTCLIENT; }
    case WM_SIZE: layout_items(); InvalidateRect(h, NULL, FALSE); return 0;
    case WM_CTLCOLOREDIT: { Pal p = pal(); SetTextColor((HDC)w, p.text); SetBkColor((HDC)w, p.ctl);
        static HBRUSH br; static COLORREF last = (COLORREF)-1; if (last != p.ctl) { if (br) DeleteObject(br); br = CreateSolidBrush(p.ctl); last = p.ctl; } return (LRESULT)br; }
    case WM_MEASUREITEM: case WM_DRAWITEM: mr = app_menu_msg(m, w, l, &hd); if (hd) return mr; break;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(l), y = (short)HIWORD(l), W, H; client_size(&W, &H);
        if (g_drag >= 0) { slider_set(g_drag, x); if (GetTickCount() - g_last_live > 110) { app_apply_all(); g_last_live = GetTickCount(); } InvalidateRect(h, NULL, FALSE); return 0; }
        int part, idx = hit_item(x, y, &part), nh = nav_hit(x, y), hov = idx;
        if (y < cap_h()) { hov = x >= W - px(46) ? -2 : (x >= W - px(92) ? -3 : -1); idx = hov; part = -1; }
        if (hov != g_hover || part != g_hover_part || nh != g_nav_hover) { g_hover = hov; g_hover_part = part; g_nav_hover = nh; InvalidateRect(h, NULL, FALSE); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 }; TrackMouseEvent(&t); return 0; }
    case WM_MOUSELEAVE: g_hover = -1; g_hover_part = -1; g_nav_hover = -1; InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONUP: {
        int x = (short)LOWORD(l), y = (short)HIWORD(l), W, H; client_size(&W, &H); int right = m == WM_RBUTTONUP;
        if (g_edit_item >= 0) end_edit(1);
        if (y < cap_h()) { if (!right) { if (x >= W - px(46)) PostMessageW(h, WM_CLOSE, 0, 0); else if (x >= W - px(92)) ShowWindow(h, SW_MINIMIZE); } return 0; }
        int ni = nav_hit(x, y);
        if (ni >= 0 && !right) { g_page = ni; g_scroll = 0; build_page(); InvalidateRect(h, NULL, FALSE); return 0; }
        int part, idx = hit_item(x, y, &part);
        if (idx >= 0) activate(idx, part, x, right);
        return 0; }
    case WM_LBUTTONUP: if (g_drag >= 0) { g_drag = -1; ReleaseCapture(); changed(); } return 0;
    case WM_MOUSEWHEEL: {
        int d = -(short)HIWORD(w) / WHEEL_DELTA * px(60); POINT pt = { (short)LOWORD(l), (short)HIWORD(l) }; ScreenToClient(h, &pt);
        if (pt.x < nav_w()) { g_nav_scroll += d; if (g_nav_scroll < 0) g_nav_scroll = 0; }
        else { g_scroll += d; int mx = g_content_h - view_h(); if (mx < 0) mx = 0; if (g_scroll > mx) g_scroll = mx; if (g_scroll < 0) g_scroll = 0; }
        InvalidateRect(h, NULL, FALSE); return 0; }
    case WM_CLOSE: end_edit(1); ShowWindow(h, SW_HIDE); g_edit_layout = 0; bar_invalidate(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void settings_open(int page) {
    if (!g_hwnd) {
        WNDCLASSW wc; memset(&wc, 0, sizeof wc); wc.lpfnWndProc = settings_proc; wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW); wc.hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(100)); wc.lpszClassName = CLASS_NAME; RegisterClassW(&wc);
        int sw, sh; screen_size(&sw, &sh); int w = px(980), hh = px(680); if (w > sw - 20) w = sw - 20; if (hh > sh - 60) hh = sh - 60;
        g_hwnd = CreateWindowExW(0, CLASS_NAME, L"WinShell Settings", WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX, (sw - w) / 2, (sh - hh) / 2 - px(10), w, hh, NULL, NULL, wc.hInstance, NULL);
        if (!g_hwnd) return;
    }
    if (page >= 0 && page < NPAGES) { g_page = page; g_scroll = 0; }
    build_page(); ShowWindow(g_hwnd, SW_SHOW); if (IsIconic(g_hwnd)) ShowWindow(g_hwnd, SW_RESTORE); SetForegroundWindow(g_hwnd);
    InvalidateRect(g_hwnd, NULL, FALSE);
}
void settings_destroy(void) { if (g_hwnd) { DestroyWindow(g_hwnd); g_hwnd = NULL; g_edit = NULL; } }

void settings_open_page(const char *name) {
    for (int i = 0; i < NPAGES; i++) if (!strcmp(PAGES[i].name, name)) { settings_open(i); return; }
    settings_open(-1);
}
