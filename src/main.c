/* WinShell v7 - Taskbar & Desktop for Winlator / Wine  (C port)
 * Usage: WinShell.exe [--restart] [--open <path>]                         */
#include "app.h"
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

static void forward_to_running(int code, const wchar_t *text) {
    HWND h = FindWindowW(L"WinShellBar", NULL);
    if (!h) return;
    COPYDATASTRUCT cds; cds.dwData = (ULONG_PTR)code;
    cds.cbData = (DWORD)((wcslen(text) + 1) * sizeof(wchar_t)); cds.lpData = (PVOID)text;
    SendMessageW(h, WM_COPYDATA, 0, (LPARAM)&cds);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)inst; (void)prev; (void)cmd; (void)show;
    int argc = 0; wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t *open_path = NULL; int restart = 0;
    for (int i = 1; i < argc; i++) {
        if (!wcscmp(argv[i], L"--restart")) restart = 1;
        else if (!wcscmp(argv[i], L"--open") && i + 1 < argc) open_path = argv[++i];
    }
    if (restart) Sleep(2000);

    /* per-monitor DPI is not needed; system DPI awareness matches the Python version */
    typedef BOOL (WINAPI *SetDpiAware_t)(void);
    SetDpiAware_t dpi = (SetDpiAware_t)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDPIAware");
    if (dpi) dpi();

    HANDLE mutex = CreateMutexW(NULL, FALSE, L"WinShell.Taskbar.Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (open_path) forward_to_running(1, open_path);
        if (argv) LocalFree(argv);
        return 0;
    }

    cfg_load(); app_save();
    apply_fonts();
    if (!bar_create()) { MessageBoxW(NULL, L"Could not create the taskbar window.", L"WinShell", MB_ICONERROR); return 1; }
    app_apply_all();
    if (open_path) app_run(L"explorer.exe", open_path, SW_SHOWNORMAL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (mutex) CloseHandle(mutex);
    if (argv) LocalFree(argv);
    return 0;
}
