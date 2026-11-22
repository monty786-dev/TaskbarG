/* Minimal Win32 stub for gcc -fsyntax-only type checking (NOT for real builds). */
#ifndef STUB_WINDOWS_H
#define STUB_WINDOWS_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#define WINAPI
#define CALLBACK
#define APIENTRY
#define CONST const
#define MAX_PATH 260
#define TRUE 1
#define FALSE 0
#define NULL_BRUSH 5
typedef int BOOL; typedef unsigned char BYTE; typedef unsigned short WORD; typedef unsigned long DWORD;
typedef unsigned int UINT; typedef long LONG; typedef short SHORT; typedef unsigned short USHORT; typedef unsigned long ULONG;
typedef void *PVOID, *LPVOID, *HANDLE; typedef const void *LPCVOID;
typedef intptr_t INT_PTR, LONG_PTR, LRESULT, LPARAM; typedef uintptr_t UINT_PTR, ULONG_PTR, WPARAM, DWORD_PTR;
typedef unsigned int UINT32; typedef unsigned long long ULONGLONG; typedef long long LONGLONG;
typedef wchar_t WCHAR, *LPWSTR; typedef const wchar_t *LPCWSTR; typedef char *LPSTR; typedef const char *LPCSTR;
typedef DWORD COLORREF; typedef WORD ATOM; typedef BYTE *LPBYTE; typedef const BYTE *PCBYTE;
typedef DWORD *LPDWORD; typedef unsigned int *PUINT; typedef int *LPINT; typedef LONG *PLONG;
#define DECL_H(n) typedef struct n##__ *n
DECL_H(HWND); DECL_H(HDC); DECL_H(HBITMAP); DECL_H(HBRUSH); DECL_H(HPEN); DECL_H(HFONT); DECL_H(HRGN);
DECL_H(HICON); DECL_H(HCURSOR); DECL_H(HINSTANCE); DECL_H(HMENU); DECL_H(HKEY);
DECL_H(HPALETTE); DECL_H(HMONITOR); DECL_H(HHOOK); DECL_H(HGLOBAL);
typedef void *HGDIOBJ; typedef HINSTANCE HMODULE; typedef int INT; typedef void *HLOCAL;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct { LONG cx, cy; } SIZE;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct { UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground; LPCWSTR lpszMenuName, lpszClassName; } WNDCLASSW;
typedef struct { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore, fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT;
typedef struct { DWORD cbSize, dwFlags; HWND hwndTrack; DWORD dwHoverTime; } TRACKMOUSEEVENT;
typedef struct { BOOL fIcon; DWORD xHotspot, yHotspot; HBITMAP hbmMask, hbmColor; } ICONINFO;
typedef struct { LONG bmType, bmWidth, bmHeight, bmWidthBytes; WORD bmPlanes, bmBitsPixel; LPVOID bmBits; } BITMAP;
typedef struct { DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount; DWORD biCompression, biSizeImage; LONG biXPelsPerMeter, biYPelsPerMeter; DWORD biClrUsed, biClrImportant; } BITMAPINFOHEADER;
typedef struct { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
typedef struct { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO;
typedef struct { BYTE BlendOp, BlendFlags, SourceConstantAlpha, AlphaFormat; } BLENDFUNCTION;
typedef struct { HICON hIcon; int iIcon; DWORD dwAttributes; WCHAR szDisplayName[MAX_PATH]; WCHAR szTypeName[80]; } SHFILEINFOW;
typedef struct { DWORD dwData; DWORD cbData; PVOID lpData; } COPYDATASTRUCT_;
typedef struct { ULONG_PTR dwData; DWORD cbData; PVOID lpData; } COPYDATASTRUCT;
typedef struct { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; DWORD nFileSizeHigh, nFileSizeLow; } WIN32_FILE_ATTRIBUTE_DATA;
typedef struct { DWORD dwFileAttributes; FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime; DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1; WCHAR cFileName[MAX_PATH]; WCHAR cAlternateFileName[14]; } WIN32_FIND_DATAW;
typedef struct { LONG Bias; WCHAR StandardName[32]; WORD st1[8]; LONG StandardBias; WCHAR DaylightName[32]; WORD st2[8]; LONG DaylightBias; } TIME_ZONE_INFORMATION;
typedef struct { DWORD lStructSize; HWND hwndOwner; HINSTANCE hInstance; LPCWSTR lpstrFilter; LPWSTR lpstrCustomFilter; DWORD nMaxCustFilter, nFilterIndex; LPWSTR lpstrFile; DWORD nMaxFile; LPWSTR lpstrFileTitle; DWORD nMaxFileTitle; LPCWSTR lpstrInitialDir, lpstrTitle; DWORD Flags; WORD nFileOffset, nFileExtension; LPCWSTR lpstrDefExt; LPARAM lCustData; void *lpfnHook; LPCWSTR lpTemplateName; } OPENFILENAMEW;
typedef struct { DWORD lStructSize; HWND hwndOwner; HWND hInstance; COLORREF rgbResult; COLORREF *lpCustColors; DWORD Flags; LPARAM lCustData; void *lpfnHook; LPCWSTR lpTemplateName; } CHOOSECOLORW;
typedef struct { LONG x_, y_; } POINTS_;
typedef struct { UINT length; UINT flags; UINT showCmd; POINT ptMinPosition, ptMaxPosition; RECT rcNormalPosition; } WINDOWPLACEMENT;
typedef struct { HWND hwnd, hwndInsertAfter; int x, y, cx, cy; UINT flags; } WINDOWPOS;
typedef struct { DWORD cbSize; RECT rcWindow, rcClient; DWORD dwStyle, dwExStyle, dwWindowStatus; UINT cxWindowBorders, cyWindowBorders; ATOM atomWindowType; WORD wCreatorVersion; } WINDOWINFO;
typedef struct { DWORD cbSize; RECT rcTitleBar; DWORD rgstate[6]; } TITLEBARINFO;
typedef struct { LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight; BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily; WCHAR lfFaceName[32]; } LOGFONTW;
typedef struct { DWORD cbSize; int iBorderWidth, iScrollWidth, iScrollHeight, iCaptionWidth, iCaptionHeight; LOGFONTW lfCaptionFont; int iSmCaptionWidth, iSmCaptionHeight; LOGFONTW lfSmCaptionFont; int iMenuWidth, iMenuHeight; LOGFONTW lfMenuFont, lfStatusFont, lfMessageFont; int iPaddedBorderWidth; } NONCLIENTMETRICSW;
typedef struct { DWORD cbSize; DWORD fMask; DWORD dwStyle; UINT cyMax; HBRUSH hbrBack; DWORD dwContextHelpID; ULONG_PTR dwMenuData; } MENUINFO;
typedef struct { HKEY dummy; } REGSTUB;
typedef struct { DWORD cb; LPWSTR lpReserved, lpDesktop, lpTitle; DWORD d[8]; WORD w[2]; BYTE *p; HANDLE h[3]; } STARTUPINFOW;
typedef struct { LONG lfX; } LOGSTUB;
typedef DWORD (WINAPI *PROC_STUB)(void);
typedef INT_PTR (WINAPI *FARPROC)(void);
typedef BOOL (CALLBACK *WNDENUMPROC)(HWND, LPARAM);
typedef void (CALLBACK *TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);

#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))
#define RGB(r,g,b) ((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))
#define GetRValue(c) ((BYTE)(c))
#define GetGValue(c) ((BYTE)(((WORD)(c))>>8))
#define GetBValue(c) ((BYTE)((c)>>16))
#define MAKELPARAM(l,h) ((LPARAM)(DWORD)(((WORD)(l))|(((DWORD)((WORD)(h)))<<16)))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#include "consts.h"
#include "api.h"
#endif
