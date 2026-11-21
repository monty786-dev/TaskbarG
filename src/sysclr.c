/* Wine / Windows system colours: window, controls, menus, tooltips, scrollbars, selection ...
 * Applied live with SetSysColors and written to HKCU\Control Panel\Colors so new programs get them. */
#include "app.h"
#include <stdio.h>
#include <string.h>

typedef struct { const char *key, *label, *reg; int id; const char *group; } Slot;
static const Slot SLOTS[] = {
    { "window", "Window background", "Window", 5, "Window and text" },
    { "windowtext", "Window text", "WindowText", 8, "Window and text" },
    { "windowframe", "Window frame / border", "WindowFrame", 6, "Window and text" },
    { "graytext", "Disabled text", "GrayText", 17, "Window and text" },
    { "background", "Desktop background", "Background", 1, "Window and text" },
    { "appworkspace", "Application workspace (MDI area)", "AppWorkspace", 12, "Window and text" },
    { "btnface", "Buttons / dialogs face (OK, Cancel, Next, Previous)", "ButtonFace", 15, "Buttons and controls" },
    { "btntext", "Button / checkbox / radio text", "ButtonText", 18, "Buttons and controls" },
    { "btnhighlight", "Button highlight edge", "ButtonHilight", 20, "Buttons and controls" },
    { "btnshadow", "Button shadow edge", "ButtonShadow", 16, "Buttons and controls" },
    { "dkshadow", "Button dark shadow (frames, tickbox, drop-down)", "ButtonDkShadow", 21, "Buttons and controls" },
    { "light", "Button light edge", "ButtonLight", 22, "Buttons and controls" },
    { "scrollbar", "Scroll bar", "Scrollbar", 0, "Buttons and controls" },
    { "menu", "Menu background", "Menu", 4, "Menus" },
    { "menutext", "Menu text", "MenuText", 7, "Menus" },
    { "menubar", "Menu bar", "MenuBar", 30, "Menus" },
    { "menuhilight", "Menu hover / flat menu highlight", "MenuHilight", 29, "Menus" },
    { "infobk", "Tooltip background", "InfoWindow", 24, "Tooltips" },
    { "infotext", "Tooltip text", "InfoText", 23, "Tooltips" },
    { "highlight", "Selection / progress bar / list highlight", "Hilight", 13, "Selection" },
    { "highlighttext", "Selection text", "HilightText", 14, "Selection" },
    { "hotlight", "Hot-tracking (hover) colour", "HotTrackingColor", 26, "Selection" },
    { "activecaption", "Active title bar", "ActiveTitle", 2, "Native title bars" },
    { "captiontext", "Active title text", "TitleText", 9, "Native title bars" },
    { "gradientactive", "Active title gradient end", "GradientActiveTitle", 27, "Native title bars" },
    { "inactivecaption", "Inactive title bar", "InactiveTitle", 3, "Native title bars" },
    { "inactivecaptiontext", "Inactive title text", "InactiveTitleText", 19, "Native title bars" },
    { "gradientinactive", "Inactive title gradient end", "GradientInactiveTitle", 28, "Native title bars" },
    { "activeborder", "Active window border", "ActiveBorder", 10, "Native title bars" },
    { "inactiveborder", "Inactive window border", "InactiveBorder", 11, "Native title bars" },
};
#define NSLOT ((int)(sizeof SLOTS / sizeof SLOTS[0]))

typedef struct { const char *name, *label; int face[3], window[3], text[3], accent[3]; } Preset;
static const Preset PRE[] = {
 {"windows98","Windows 98 / Classic",{192,192,192},{255,255,255},{0,0,0},{0,0,128}},
 {"windows2000","Windows 2000",{212,208,200},{255,255,255},{0,0,0},{10,36,106}},
 {"windowsxp","Windows XP (Luna)",{236,233,216},{255,255,255},{0,0,0},{49,106,197}},
 {"windows7","Windows 7",{240,240,240},{255,255,255},{0,0,0},{51,153,255}},
 {"windows8","Windows 8",{240,240,240},{255,255,255},{0,0,0},{0,120,215}},
 {"windows10","Windows 10 light",{240,240,240},{255,255,255},{0,0,0},{0,120,215}},
 {"windows10dark","Windows 10 dark",{43,43,43},{32,32,32},{255,255,255},{0,120,215}},
 {"windows11","Windows 11 light",{243,243,243},{255,255,255},{0,0,0},{0,103,192}},
 {"windows11dark","Windows 11 dark",{44,44,44},{32,32,32},{255,255,255},{76,194,255}},
 {"hcblack","High contrast black",{0,0,0},{0,0,0},{255,255,255},{26,235,255}},
 {"hcwhite","High contrast white",{255,255,255},{255,255,255},{0,0,0},{55,0,110}},
 {"macoslight","macOS light",{236,236,236},{255,255,255},{30,30,30},{0,122,255}},
 {"macosdark","macOS dark",{40,40,40},{30,30,30},{235,235,235},{10,132,255}},
 {"ubuntu","Ubuntu (Yaru dark)",{60,60,60},{48,48,48},{255,255,255},{233,84,32}},
 {"mint","Linux Mint (Mint-Y)",{232,232,232},{255,255,255},{40,40,40},{143,168,98}},
 {"kde","KDE Breeze",{239,240,241},{255,255,255},{35,38,41},{61,174,233}},
 {"gnome","GNOME Adwaita",{237,237,237},{255,255,255},{46,52,54},{53,132,228}},
 {"solarized","Solarized dark",{7,54,66},{0,43,54},{147,161,161},{38,139,210}},
 {"nord","Nord",{59,66,82},{46,52,64},{236,239,244},{136,192,208}},
 {"dracula","Dracula",{68,71,90},{40,42,54},{248,248,242},{189,147,249}},
};
int sys_preset_count(void) { return (int)(sizeof PRE / sizeof PRE[0]); }
const char *sys_preset_name(int i) { return PRE[i].name; }
const char *sys_preset_label(int i) { return PRE[i].label; }
int sys_slot_count(void) { return NSLOT; }
const char *sys_slot_key(int i) { return SLOTS[i].key; }
const char *sys_slot_label(int i) { return SLOTS[i].label; }
const char *sys_slot_group(int i) { return SLOTS[i].group; }

static COLORREF C3(const int *v) { return RGB(v[0], v[1], v[2]); }

static COLORREF derive(const Preset *p, const char *key) {
    COLORREF face = C3(p->face), win = C3(p->window), txt = C3(p->text), acc = C3(p->accent);
    int dark = c_luminance(face) < 128, hc = !strcmp(p->name, "hcblack");
    COLORREF white = RGB(255, 255, 255), black = RGB(0, 0, 0);
    COLORREF atext = c_luminance(acc) > 150 ? black : white;
    #define IS(k) !strcmp(key, k)
    if (IS("window")) return win; if (IS("windowtext") || IS("btntext") || IS("menutext")) return txt;
    if (IS("btnface") || IS("menu") || IS("menubar") || IS("activeborder") || IS("inactiveborder")) return face;
    if (IS("btnhighlight")) return dark ? c_mix(face, white, hc ? 0.0 : 0.22) : (c_luminance(face) > 250 ? RGB(235, 235, 235) : white);
    if (IS("btnshadow")) return c_mix(face, hc ? white : black, hc ? 0.5 : 0.38);
    if (IS("dkshadow")) return dark ? c_mix(face, black, 0.6) : c_mix(face, black, 0.7);
    if (IS("light")) return c_mix(face, white, dark ? 0.15 : 0.4);
    if (IS("windowframe")) return dark ? c_mix(face, white, 0.25) : c_mix(face, black, 0.75);
    if (IS("graytext")) return c_mix(txt, face, 0.55);
    if (IS("scrollbar")) return dark ? c_mix(face, white, 0.06) : c_mix(face, white, 0.5);
    if (IS("background")) return c_mix(acc, black, 0.55);
    if (IS("appworkspace")) return dark ? c_mix(face, black, 0.35) : c_mix(face, black, 0.22);
    if (IS("highlight") || IS("menuhilight") || IS("activecaption")) return acc;
    if (IS("highlighttext") || IS("captiontext")) return atext;
    if (IS("hotlight")) return c_mix(acc, white, 0.25);
    if (IS("gradientactive")) return c_mix(acc, white, 0.35);
    if (IS("infobk")) return dark ? c_mix(face, white, 0.12) : RGB(255, 255, 225);
    if (IS("infotext")) return dark ? txt : black;
    if (IS("inactivecaption")) return dark ? c_mix(face, black, 0.2) : c_mix(face, black, 0.12);
    if (IS("inactivecaptiontext")) return c_mix(txt, face, 0.4);
    if (IS("gradientinactive")) return dark ? c_mix(face, white, 0.1) : c_mix(face, white, 0.3);
    #undef IS
    return face;
}

static const Preset *cur_preset(void) {
    const char *n = cfg_str("ctrl_preset");
    for (int i = 0; i < sys_preset_count(); i++) if (!strcmp(PRE[i].name, n)) return &PRE[i];
    return &PRE[5];
}

COLORREF sys_slot_color(int i) {
    JNode *ov = json_get(cfg_node("ctrl_colors"), SLOTS[i].key);
    if (ov && ov->t == J_STR && ov->s && ov->s[0]) return c_hex(ov->s, derive(cur_preset(), SLOTS[i].key));
    return derive(cur_preset(), SLOTS[i].key);
}
int sys_slot_overridden(int i) { JNode *ov = json_get(cfg_node("ctrl_colors"), SLOTS[i].key); return ov && ov->t == J_STR && ov->s && ov->s[0]; }
void sys_slot_set(int i, const char *hex) { JNode *o = cfg_node("ctrl_colors"); if (o) { if (hex && hex[0]) json_put(o, SLOTS[i].key, json_str(hex)); else json_remove(o, SLOTS[i].key); } }
void sys_preset_apply(int i) { cfg_set_str("ctrl_preset", PRE[i].name); JNode *o = cfg_node("ctrl_colors"); if (o) { json_free(o->child); o->child = NULL; } cfg_set_bool("ctrl_enabled", 1); }

static COLORREF g_backup[32]; static int g_have_backup[32];

void sys_restore(void) {
    int ids[32], n = 0; COLORREF vals[32];
    for (int i = 0; i < 32; i++) if (g_have_backup[i]) { ids[n] = i; vals[n++] = g_backup[i]; g_have_backup[i] = 0; }
    if (n) SetSysColors(n, ids, vals);
}

static void write_registry(int n, const int *ids, const COLORREF *vals) {
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Colors", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL) != ERROR_SUCCESS) return;
    for (int i = 0; i < n; i++) {
        for (int s = 0; s < NSLOT; s++) if (SLOTS[s].id == ids[i]) {
            wchar_t name[48], v[32]; w_from_utf8_buf(SLOTS[s].reg, name, 48);
            _snwprintf(v, 32, L"%d %d %d", GetRValue(vals[i]), GetGValue(vals[i]), GetBValue(vals[i]));
            RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
        }
    }
    RegCloseKey(k);
}

void sys_apply(void) {
    if (!cfg_bool("ctrl_enabled")) { sys_restore(); return; }
    int ids[32], n = 0; COLORREF vals[32];
    for (int i = 0; i < NSLOT; i++) {
        int id = SLOTS[i].id;
        if (!g_have_backup[id]) { g_backup[id] = GetSysColor(id); g_have_backup[id] = 1; }
        ids[n] = id; vals[n++] = sys_slot_color(i);
    }
    SetSysColors(n, ids, vals); write_registry(n, ids, vals);
}
