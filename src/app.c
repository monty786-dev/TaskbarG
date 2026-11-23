#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ULONGLONG g_mtime;
typedef BOOL (WINAPI *ShellHook_t)(HWND);

ULONGLONG app_config_mtime(void) { return g_mtime; }
void app_save(void) { cfg_save(); g_mtime = cfg_mtime(); }

void app_toggle_desktop(void) { cfg_set_bool("full_desktop", !cfg_bool("full_desktop")); app_save(); app_apply_all(); }

void app_run(const wchar_t *target, const wchar_t *params, int show) {
    if (!target || !target[0]) return;
    if (!wcsncmp(target, L"winshell:", 9)) {                       /* built-in "apps" */
        const wchar_t *k = target + 9;
        if (!wcsncmp(k, L"settings", 8)) { if (k[8] == L':') { char *u = utf8_from_w(k + 9); settings_open_page(u); free(u); } else settings_open(-1); }
        else if (!wcscmp(k, L"game")) game_open(); else if (!wcscmp(k, L"desktop")) app_toggle_desktop(); else if (!wcscmp(k, L"layout")) editor_open(0);
        return;
    }
    if (!wcsncmp(target, L"fake:", 5)) {                           /* icon-only apps: optionally forward to a real target */
        JNode *f = fake_app(target); JNode *t = f ? json_get(f, "target") : NULL;
        if (t && t->s && t->s[0]) { wchar_t w[MAX_PATH]; w_from_utf8_buf(t->s, w, MAX_PATH); if (wcsncmp(w, L"fake:", 5) != 0) app_run(w, params, show); }
        return;
    }
    wchar_t full[MAX_PATH];
    /* resolve bare names like "notepad.exe" the same way the Python app does */
    find_exe(target, full, MAX_PATH);
    HINSTANCE r = ShellExecuteW(NULL, L"open", full, params, NULL, show ? show : SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32 && wcscmp(full, target) != 0)
        ShellExecuteW(NULL, L"open", target, params, NULL, show ? show : SW_SHOWNORMAL);
}

/* ---- themed popup menus: owner-drawn so they follow the selected theme ---- */
typedef struct { wchar_t label[128]; int grayed, checked; } MenuItem;
static MenuItem g_mi[128]; static int g_nmi; static HBRUSH g_menu_brush;

int app_popup(HWND owner, const wchar_t **labels, const int *ids, const int *flags, int n,
              UINT tpm_flags, int x, int y) {
    HMENU m = CreatePopupMenu(); POINT pt; GetCursorPos(&pt); Theme th = app_theme(1);
    if (g_menu_brush) DeleteObject(g_menu_brush);
    g_menu_brush = CreateSolidBrush(th.bg);
    MENUINFO mi; memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi; mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS; mi.hbrBack = g_menu_brush; SetMenuInfo(m, &mi);
    g_nmi = 0;
    for (int i = 0; i < n && g_nmi < 128; i++) {
        if (!labels[i]) { AppendMenuW(m, MF_SEPARATOR, 0, NULL); continue; }
        MenuItem *it = &g_mi[g_nmi++]; lstrcpynW(it->label, labels[i], 128);
        it->grayed = flags && (flags[i] & MF_GRAYED); it->checked = flags && (flags[i] & MF_CHECKED);
        AppendMenuW(m, MF_OWNERDRAW | (it->grayed ? MF_GRAYED : 0), (UINT_PTR)ids[i], (LPCWSTR)it);
    }
    SetForegroundWindow(owner); g_menu_depth++;
    int cmd = (int)TrackPopupMenu(m, tpm_flags, x == POPUP_CUR ? pt.x : x, y == POPUP_CUR ? pt.y : y, 0, owner, NULL);
    g_menu_depth--; DestroyMenu(m);
    return cmd;
}

LRESULT app_menu_msg(UINT msg, WPARAM wp, LPARAM lp, int *handled) {
    *handled = 0;
    if (msg == WM_MEASUREITEM) {
        MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)lp; MenuItem *it = (MenuItem *)mi->itemData;
        if (mi->CtlType != ODT_MENU || it < g_mi || it >= g_mi + 128) return 0;
        HDC dc = GetDC(NULL); HGDIOBJ o = SelectObject(dc, g_font); SIZE sz = { 0, 0 };
        GetTextExtentPoint32W(dc, it->label, lstrlenW(it->label), &sz); SelectObject(dc, o); ReleaseDC(NULL, dc);
        mi->itemWidth = (UINT)(sz.cx + px(64)); mi->itemHeight = (UINT)px(30); *handled = 1; return TRUE;
    }
    if (msg == WM_DRAWITEM) {
        DRAWITEMSTRUCT *d = (DRAWITEMSTRUCT *)lp; MenuItem *it = (MenuItem *)d->itemData;
        if (d->CtlType != ODT_MENU || it < g_mi || it >= g_mi + 128) return 0;
        Theme th = app_theme(1); int sel = (d->itemState & ODS_SELECTED) && !it->grayed;
        g_fill(d->hDC, d->rcItem, sel ? c_mix(th.bg, th.accent, 0.38) : th.bg);
        SetBkMode(d->hDC, TRANSPARENT); HGDIOBJ o = SelectObject(d->hDC, g_font);
        RECT t = { d->rcItem.left + px(34), d->rcItem.top, d->rcItem.right - px(10), d->rcItem.bottom };
        g_text(d->hDC, it->label, t, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX, it->grayed ? th.dim : th.text);
        if (it->checked) { RECT c = { d->rcItem.left + px(8), d->rcItem.top + px(7), d->rcItem.left + px(24), d->rcItem.bottom - px(7) }; glyph_draw(G_CHEVRON_D, d->hDC, c, th.accent); }
        SelectObject(d->hDC, o); *handled = 1; return TRUE;
    }
    (void)wp; return 0;
}

void apply_alpha(HWND h, int a) {
    LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) SetWindowLongPtrW(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    if (a < 1) a = 1; if (a > 255) a = 255;
    SetLayeredWindowAttributes(h, 0, (BYTE)a, LWA_ALPHA);
}

int app_confirm(HWND owner, const wchar_t *title, const wchar_t *text) {
    g_menu_depth++;
    int r = MessageBoxW(owner, text, title, MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST);
    g_menu_depth--;
    return r == IDOK;
}

void app_record_use(const char *path) {
    JNode *usage = cfg_node("app_usage"), *recent = cfg_node("app_recent");
    if (usage && usage->t == J_OBJ) {
        JNode *cur = json_get(usage, path);
        json_put(usage, path, json_num((cur ? cur->n : 0) + 1, 1));
    }
    if (recent && recent->t == J_ARR) {
        const char *list[11]; int n = 0; list[n++] = path;
        for (JNode *c = recent->child; c && n < 10; c = c->next) if (c->s && strcmp(c->s, path) != 0) list[n++] = c->s;
        char *copy[11]; for (int i = 0; i < n; i++) copy[i] = _strdup(list[i]);   /* list points into the node we replace */
        cfg_list_set("app_recent", (const char **)copy, n);
        for (int i = 0; i < n; i++) free(copy[i]);
    }
    app_save();
}

static void set_autostart(void) {
    HKEY k; wchar_t exe[MAX_PATH], cmd[MAX_PATH + 4];
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    if (cfg_bool("autostart")) {
        GetModuleFileNameW(NULL, exe, MAX_PATH); _snwprintf(cmd, MAX_PATH + 4, L"\"%ls\"", exe);
        RegSetValueExW(k, L"WinShell", 0, REG_SZ, (const BYTE *)cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(k, L"WinShell");
    RegCloseKey(k);
}

void app_apply_all(void) {
    apply_fonts(); g_glass_clear(); img_clear();
    bar_apply(); start_apply(); skin_apply(); reveal_apply(); desktop_apply(); game_apply(); sys_apply(); set_autostart(); settings_refresh(); if (editor_hwnd() && IsWindowVisible(editor_hwnd())) InvalidateRect(editor_hwnd(), NULL, FALSE);
}

void app_reload(void) {
    cfg_load(); g_mtime = cfg_mtime(); app_apply_all();
}

void app_restart(void) {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
    ShellExecuteW(NULL, L"open", exe, L"--restart", NULL, SW_SHOWNORMAL);
    PostMessageW(g_bar_hwnd, WM_CLOSE, 0, 0);
}

void app_shutdown(void) {
    ShellHook_t dereg = (ShellHook_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "DeregisterShellHookWindow");
    if (dereg && g_bar_hwnd) dereg(g_bar_hwnd);
    bar_restore_work_area();
    HWND wine = FindWindowW(L"Shell_TrayWnd", NULL);
    if (wine) ShowWindow(wine, SW_SHOW);
    skin_shutdown(); reveal_destroy(); desktop_destroy(); game_destroy(); preview_destroy(); editor_destroy(); settings_destroy(); start_destroy(); tray_close(); g_glass_clear(); icons_close();
    if (g_font) DeleteObject(g_font);
    if (g_font_big) DeleteObject(g_font_big);
    if (g_font_small) DeleteObject(g_font_small);
    PostQuitMessage(0);
}
