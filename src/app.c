#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ULONGLONG g_mtime;
typedef BOOL (WINAPI *ShellHook_t)(HWND);

ULONGLONG app_config_mtime(void) { return g_mtime; }
void app_save(void) { cfg_save(); g_mtime = cfg_mtime(); }

void app_run(const wchar_t *target, const wchar_t *params, int show) {
    if (!target || !target[0]) return;
    wchar_t full[MAX_PATH];
    /* resolve bare names like "notepad.exe" the same way the Python app does */
    find_exe(target, full, MAX_PATH);
    HINSTANCE r = ShellExecuteW(NULL, L"open", full, params, NULL, show ? show : SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32 && wcscmp(full, target) != 0)
        ShellExecuteW(NULL, L"open", target, params, NULL, show ? show : SW_SHOWNORMAL);
}

int app_popup(HWND owner, const wchar_t **labels, const int *ids, const int *flags, int n,
              UINT tpm_flags, int x, int y) {
    HMENU m = CreatePopupMenu(); POINT pt; GetCursorPos(&pt);
    for (int i = 0; i < n; i++) {
        if (!labels[i]) { AppendMenuW(m, MF_SEPARATOR, 0, NULL); continue; }
        wchar_t buf[256]; int k = 0;                      /* escape '&' so titles never become mnemonics */
        for (const wchar_t *p = labels[i]; *p && k < 253; p++) { if (*p == L'&') buf[k++] = L'&'; buf[k++] = *p; }
        buf[k] = 0;
        AppendMenuW(m, MF_STRING | (UINT)(flags ? flags[i] : 0), (UINT_PTR)ids[i], buf);
    }
    SetForegroundWindow(owner); g_menu_depth++;
    int cmd = (int)TrackPopupMenu(m, tpm_flags, x == POPUP_CUR ? pt.x : x, y == POPUP_CUR ? pt.y : y, 0, owner, NULL);
    g_menu_depth--; DestroyMenu(m);
    return cmd;
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
    apply_fonts(); g_glass_clear();
    bar_apply(); start_apply(); set_autostart();
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
    start_destroy(); tray_close(); g_glass_clear(); icons_close();
    if (g_font) DeleteObject(g_font);
    if (g_font_big) DeleteObject(g_font_big);
    if (g_font_small) DeleteObject(g_font_small);
    PostQuitMessage(0);
}
