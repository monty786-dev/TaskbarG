/* function prototypes for syntax checking only */
#include <stdio.h>
#define W_(ret) ret WINAPI
#define LP LPARAM
#define DECLARE(x) x
/* kernel32 */
W_(DWORD) GetLastError(void); W_(void) SetLastError(DWORD); W_(void) Sleep(DWORD); W_(DWORD) GetTickCount(void);
W_(HMODULE) GetModuleHandleW(LPCWSTR); W_(FARPROC) GetProcAddress(HMODULE, LPCSTR); W_(HMODULE) LoadLibraryW(LPCWSTR); W_(BOOL) FreeLibrary(HMODULE);
W_(DWORD) GetModuleFileNameW(HMODULE, LPWSTR, DWORD); W_(HANDLE) CreateMutexW(void *, BOOL, LPCWSTR); W_(BOOL) CloseHandle(HANDLE);
W_(DWORD) GetEnvironmentVariableW(LPCWSTR, LPWSTR, DWORD); W_(DWORD) ExpandEnvironmentStringsW(LPCWSTR, LPWSTR, DWORD);
W_(BOOL) CreateDirectoryW(LPCWSTR, void *); W_(DWORD) GetFileAttributesW(LPCWSTR); W_(BOOL) GetFileAttributesExW(LPCWSTR, int, void *);
W_(HANDLE) CreateFileW(LPCWSTR, DWORD, DWORD, void *, DWORD, DWORD, HANDLE); W_(BOOL) ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, void *); W_(BOOL) WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, void *);
W_(HANDLE) FindFirstFileW(LPCWSTR, WIN32_FIND_DATAW *); W_(BOOL) FindNextFileW(HANDLE, WIN32_FIND_DATAW *); W_(BOOL) FindClose(HANDLE);
W_(int) MultiByteToWideChar(UINT, DWORD, LPCSTR, int, LPWSTR, int); W_(int) WideCharToMultiByte(UINT, DWORD, LPCWSTR, int, LPSTR, int, LPCSTR, BOOL *);
W_(LPWSTR) lstrcpyW(LPWSTR, LPCWSTR); W_(LPWSTR) lstrcpynW(LPWSTR, LPCWSTR, int); W_(LPSTR) lstrcpynA(LPSTR, LPCSTR, int); W_(int) lstrlenW(LPCWSTR);
W_(DWORD) GetTimeZoneInformation(TIME_ZONE_INFORMATION *); W_(HANDLE) OpenProcess(DWORD, BOOL, DWORD);
W_(BOOL) QueryFullProcessImageNameW(HANDLE, DWORD, LPWSTR, LPDWORD); W_(HANDLE) LocalFree(HANDLE);
W_(LPWSTR) GetCommandLineW(void); W_(DWORD) GetCurrentProcessId(void);
/* user32 */
W_(ATOM) RegisterClassW(const WNDCLASSW *);
W_(HWND) CreateWindowExW(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
W_(BOOL) DestroyWindow(HWND); W_(LRESULT) DefWindowProcW(HWND, UINT, WPARAM, LPARAM); W_(LRESULT) CallWindowProcW(WNDPROC, HWND, UINT, WPARAM, LPARAM);
W_(BOOL) GetMessageW(MSG *, HWND, UINT, UINT); W_(BOOL) TranslateMessage(const MSG *); W_(LRESULT) DispatchMessageW(const MSG *); W_(void) PostQuitMessage(int);
W_(BOOL) PostMessageW(HWND, UINT, WPARAM, LPARAM); W_(LRESULT) SendMessageW(HWND, UINT, WPARAM, LPARAM);
W_(LRESULT) SendMessageTimeoutW(HWND, UINT, WPARAM, LPARAM, UINT, UINT, DWORD_PTR *);
W_(UINT) RegisterWindowMessageW(LPCWSTR); W_(UINT_PTR) SetTimer(HWND, UINT_PTR, UINT, TIMERPROC); W_(BOOL) KillTimer(HWND, UINT_PTR);
W_(BOOL) ShowWindow(HWND, int); W_(BOOL) SetWindowPos(HWND, HWND, int, int, int, int, UINT); W_(BOOL) GetWindowRect(HWND, RECT *); W_(BOOL) GetClientRect(HWND, RECT *);
W_(BOOL) IsWindow(HWND); W_(BOOL) IsWindowVisible(HWND); W_(BOOL) IsIconic(HWND); W_(BOOL) IsZoomed(HWND);
W_(HWND) GetForegroundWindow(void); W_(BOOL) SetForegroundWindow(HWND); W_(HWND) SetFocus(HWND); W_(HWND) GetFocus(void); W_(HWND) GetAncestor(HWND, UINT); W_(HWND) GetWindow(HWND, UINT);
W_(HWND) FindWindowW(LPCWSTR, LPCWSTR); W_(BOOL) EnumWindows(WNDENUMPROC, LPARAM); W_(BOOL) EnumChildWindows(HWND, WNDENUMPROC, LPARAM);
W_(int) GetClassNameW(HWND, LPWSTR, int); W_(int) GetWindowTextW(HWND, LPWSTR, int); W_(int) GetWindowTextLengthW(HWND); W_(BOOL) SetWindowTextW(HWND, LPCWSTR);
W_(DWORD) GetWindowThreadProcessId(HWND, LPDWORD); W_(LONG_PTR) GetWindowLongPtrW(HWND, int); W_(LONG_PTR) SetWindowLongPtrW(HWND, int, LONG_PTR); W_(LONG_PTR) GetClassLongPtrW(HWND, int);
W_(int) SetWindowRgn(HWND, HRGN, BOOL); W_(BOOL) SetLayeredWindowAttributes(HWND, COLORREF, BYTE, DWORD); W_(BOOL) InvalidateRect(HWND, const RECT *, BOOL); W_(BOOL) UpdateWindow(HWND);
W_(HDC) BeginPaint(HWND, PAINTSTRUCT *); W_(BOOL) EndPaint(HWND, const PAINTSTRUCT *); W_(HDC) GetDC(HWND); W_(int) ReleaseDC(HWND, HDC);
W_(int) FillRect(HDC, const RECT *, HBRUSH); W_(int) DrawTextW(HDC, LPCWSTR, int, RECT *, UINT); W_(BOOL) DrawIconEx(HDC, int, int, HICON, int, int, UINT, HBRUSH, UINT);
W_(BOOL) DestroyIcon(HICON); W_(HICON) LoadIconW(HINSTANCE, LPCWSTR); W_(HCURSOR) LoadCursorW(HINSTANCE, LPCWSTR); W_(HANDLE) LoadImageW(HINSTANCE, LPCWSTR, UINT, int, int, UINT);
W_(BOOL) GetIconInfo(HICON, ICONINFO *); W_(HCURSOR) SetCursor(HCURSOR);
W_(BOOL) GetCursorPos(POINT *); W_(BOOL) SetCursorPos(int, int); W_(SHORT) GetAsyncKeyState(int); W_(HWND) SetCapture(HWND); W_(BOOL) ReleaseCapture(void); W_(HWND) GetCapture(void);
W_(BOOL) ScreenToClient(HWND, POINT *); W_(BOOL) ClientToScreen(HWND, POINT *); W_(BOOL) TrackMouseEvent(TRACKMOUSEEVENT *);
W_(int) GetSystemMetrics(int); W_(BOOL) SystemParametersInfoW(UINT, UINT, PVOID, UINT);
W_(HMENU) CreatePopupMenu(void); W_(BOOL) AppendMenuW(HMENU, UINT, UINT_PTR, LPCWSTR); W_(BOOL) DestroyMenu(HMENU);
W_(BOOL) TrackPopupMenu(HMENU, UINT, int, int, int, HWND, const RECT *); W_(BOOL) SetMenuInfo(HMENU, const MENUINFO *);
W_(int) MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT); W_(BOOL) PrintWindow(HWND, HDC, UINT); W_(BOOL) GetTitleBarInfo(HWND, TITLEBARINFO *); W_(BOOL) GetWindowInfo(HWND, WINDOWINFO *);
W_(BOOL) GetWindowPlacement(HWND, WINDOWPLACEMENT *); W_(BOOL) SetWindowPlacement(HWND, const WINDOWPLACEMENT *);
W_(HWND) GetDesktopWindow(void); W_(HWND) GetParent(HWND); W_(HWND) SetParent(HWND, HWND); W_(HWND) GetShellWindow(void); W_(HWND) GetTopWindow(HWND);
W_(BOOL) MoveWindow(HWND, int, int, int, int, BOOL); W_(BOOL) BringWindowToTop(HWND); W_(BOOL) FlashWindow(HWND, BOOL);
W_(BOOL) AnimateWindow(HWND, DWORD, DWORD); W_(BOOL) SetSysColors(int, const INT *, const COLORREF *); W_(DWORD) GetSysColor(int);
W_(BOOL) SetProcessDPIAware(void); W_(UINT) GetDpiForSystem(void);
W_(BOOL) OffsetRect(RECT *, int, int); W_(BOOL) InflateRect(RECT *, int, int); W_(BOOL) PtInRect(const RECT *, POINT); W_(BOOL) IntersectRect(RECT *, const RECT *, const RECT *);
W_(BOOL) EqualRect(const RECT *, const RECT *); W_(BOOL) SetRect(RECT *, int, int, int, int);
W_(int) GetKeyState(int); W_(BOOL) ShowScrollBar(HWND, int, BOOL); W_(BOOL) EnableWindow(HWND, BOOL);
W_(HWND) WindowFromPoint(POINT); W_(BOOL) IsChild(HWND, HWND); W_(HHOOK) SetWindowsHookExW(int, void *, HINSTANCE, DWORD);
typedef void (CALLBACK *WINEVENTPROC)(HANDLE, DWORD, HWND, LONG, LONG, DWORD, DWORD);
W_(HANDLE) SetWinEventHook(DWORD, DWORD, HMODULE, WINEVENTPROC, DWORD, DWORD, DWORD); W_(BOOL) UnhookWinEvent(HANDLE);
W_(LRESULT) SendNotifyMessageW(HWND, UINT, WPARAM, LPARAM); W_(BOOL) AdjustWindowRectEx(RECT *, DWORD, BOOL, DWORD);
/* gdi32 */
W_(HGDIOBJ) SelectObject(HDC, HGDIOBJ); W_(BOOL) DeleteObject(HGDIOBJ); W_(HGDIOBJ) GetStockObject(int);
W_(HDC) CreateCompatibleDC(HDC); W_(BOOL) DeleteDC(HDC); W_(HBITMAP) CreateCompatibleBitmap(HDC, int, int);
W_(HBITMAP) CreateDIBSection(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
W_(BOOL) BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD); W_(BOOL) StretchBlt(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
W_(int) SetStretchBltMode(HDC, int);
W_(BOOL) AlphaBlend(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION); W_(BOOL) TransparentBlt(HDC, int, int, int, int, HDC, int, int, int, int, UINT);
W_(HBRUSH) CreateSolidBrush(COLORREF); W_(HPEN) CreatePen(int, int, COLORREF); W_(HFONT) CreateFontW(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
W_(HRGN) CreateRectRgn(int, int, int, int); W_(HRGN) CreateRoundRectRgn(int, int, int, int, int, int); W_(HRGN) CreateEllipticRgn(int, int, int, int);
W_(int) SelectClipRgn(HDC, HRGN); W_(int) SetBkMode(HDC, int); W_(COLORREF) SetBkColor(HDC, COLORREF); W_(COLORREF) SetTextColor(HDC, COLORREF);
W_(BOOL) Ellipse(HDC, int, int, int, int); W_(BOOL) Rectangle(HDC, int, int, int, int); W_(BOOL) RoundRect(HDC, int, int, int, int, int, int);
W_(BOOL) MoveToEx(HDC, int, int, POINT *); W_(BOOL) LineTo(HDC, int, int); W_(BOOL) Arc(HDC, int, int, int, int, int, int, int, int); W_(BOOL) Polygon(HDC, const POINT *, int);
W_(COLORREF) SetPixel(HDC, int, int, COLORREF); W_(COLORREF) GetPixel(HDC, int, int);
W_(int) GetObjectW(HANDLE, int, LPVOID); W_(int) GetDIBits(HDC, HBITMAP, UINT, UINT, LPVOID, BITMAPINFO *, UINT);
W_(BOOL) GetTextExtentPoint32W(HDC, LPCWSTR, int, SIZE *); W_(BOOL) TextOutW(HDC, int, int, LPCWSTR, int);
W_(int) SetGraphicsMode(HDC, int); W_(BOOL) GdiFlush(void); W_(int) GetDeviceCaps(HDC, int);
/* shell32 / comdlg32 / advapi32 / others */
W_(HINSTANCE) ShellExecuteW(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, int);
W_(LPWSTR *) CommandLineToArgvW(LPCWSTR, int *);
W_(DWORD_PTR) SHGetFileInfoW(LPCWSTR, DWORD, SHFILEINFOW *, UINT, UINT); W_(UINT) ExtractIconExW(LPCWSTR, int, HICON *, HICON *, UINT);
W_(BOOL) GetOpenFileNameW(OPENFILENAMEW *); W_(BOOL) ChooseColorW(CHOOSECOLORW *);
W_(LONG) RegOpenKeyExW(HKEY, LPCWSTR, DWORD, DWORD, HKEY *); W_(LONG) RegCreateKeyExW(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, DWORD, void *, HKEY *, DWORD *);
W_(LONG) RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE *, DWORD); W_(LONG) RegDeleteValueW(HKEY, LPCWSTR); W_(LONG) RegCloseKey(HKEY);
W_(LONG) RegQueryValueExW(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
extern int _snwprintf(wchar_t *, size_t, const wchar_t *, ...); extern int _wcsicmp(const wchar_t *, const wchar_t *); extern int _wcsnicmp(const wchar_t *, const wchar_t *, size_t);
extern wchar_t *_wcslwr(wchar_t *); extern char *_strdup(const char *); extern FILE *_wfopen(const wchar_t *, const wchar_t *);

typedef struct { DWORD dwLength, dwMemoryLoad; ULONGLONG ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile, ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual; } MEMORYSTATUSEX;
W_(BOOL) GlobalMemoryStatusEx(MEMORYSTATUSEX *); W_(ULONGLONG) GetTickCount64(void); W_(BOOL) DeleteFileW(LPCWSTR); W_(DWORD) GetCurrentDirectoryW(DWORD, LPWSTR);

typedef struct { DWORD cbSize, fMask; HWND hwnd; LPCWSTR lpVerb, lpFile, lpParameters, lpDirectory; int nShow; HINSTANCE hInstApp; void *lpIDList; LPCWSTR lpClass; HKEY hkeyClass; DWORD dwHotKey; HANDLE hIcon; HANDLE hProcess; } SHELLEXECUTEINFOW;
W_(BOOL) ShellExecuteExW(SHELLEXECUTEINFOW *); W_(DWORD) WaitForSingleObject(HANDLE, DWORD);

W_(HBITMAP) CreateBitmap(int, int, UINT, UINT, const void *); W_(HICON) CreateIconIndirect(ICONINFO *); W_(BOOL) CopyFileW(LPCWSTR, LPCWSTR, BOOL); W_(BOOL) GetSaveFileNameW(OPENFILENAMEW *); W_(DWORD) GetFileSize(HANDLE, LPDWORD); W_(BOOL) SHCreateDirectoryExW_(void);
