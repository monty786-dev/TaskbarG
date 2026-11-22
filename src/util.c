#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <ctype.h>
#include <math.h>

/* ----------------------------------------------------------- strings */
wchar_t *w_from_utf8(const char *s) {
    if (!s) s = "";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = (wchar_t *)calloc((size_t)(n > 0 ? n : 1), sizeof(wchar_t));
    if (w && n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
void w_from_utf8_buf(const char *s, wchar_t *out, int n) {
    if (n <= 0) return;
    out[0] = 0;
    if (!s) return;
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n)) out[n - 1] = 0;
}
char *utf8_from_w(const wchar_t *s) {
    if (!s) s = L"";
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    char *d = (char *)calloc((size_t)(n > 0 ? n : 1), 1);
    if (d && n > 0) WideCharToMultiByte(CP_UTF8, 0, s, -1, d, n, NULL, NULL);
    return d;
}

/* ----------------------------------------------------------- colours */
static int clamp255(double v) { return v < 0 ? 0 : (v > 255 ? 255 : (int)v); }
COLORREF c_shade(COLORREF c, double f) {
    return RGB(clamp255(GetRValue(c) * f), clamp255(GetGValue(c) * f), clamp255(GetBValue(c) * f));
}
COLORREF c_mix(COLORREF a, COLORREF b, double t) {
    return RGB((int)(GetRValue(a) * (1 - t) + GetRValue(b) * t),
               (int)(GetGValue(a) * (1 - t) + GetGValue(b) * t),
               (int)(GetBValue(a) * (1 - t) + GetBValue(b) * t));
}
double c_luminance(COLORREF c) {
    return 0.299 * GetRValue(c) + 0.587 * GetGValue(c) + 0.114 * GetBValue(c);
}
COLORREF c_hex(const char *text, COLORREF def) {
    if (!text) return def;
    while (*text == '#') text++;
    if (strlen(text) < 6) return def;
    unsigned r, g, b; char tmp[3] = { 0, 0, 0 }; char *end;
    for (int i = 0; i < 6; i++) if (!isxdigit((unsigned char)text[i])) return def;
    tmp[0] = text[0]; tmp[1] = text[1]; r = (unsigned)strtoul(tmp, &end, 16);
    tmp[0] = text[2]; tmp[1] = text[3]; g = (unsigned)strtoul(tmp, &end, 16);
    tmp[0] = text[4]; tmp[1] = text[5]; b = (unsigned)strtoul(tmp, &end, 16);
    return RGB(r, g, b);
}

/* ------------------------------------------------------------- clock */
static int zone_offset_s(void) {
    const char *mode = cfg_str("clock_timezone");
    if (!strcmp(mode, "system")) {
        TIME_ZONE_INFORMATION tz; DWORD r = GetTimeZoneInformation(&tz);
        LONG bias = tz.Bias + (r == TIME_ZONE_ID_DAYLIGHT ? tz.DaylightBias : tz.StandardBias);
        return -(int)bias * 60;
    }
    if (!strcmp(mode, "custom")) return cfg_int("clock_utc_offset") * 60;
    return IST_OFFSET;
}

void format_clock(wchar_t *out, int n) {
    wchar_t *fmt = w_from_utf8(cfg_str("clock_format"));
    time_t t = time(NULL) + (time_t)cfg_num("clock_offset") + zone_offset_s();
    struct tm *tp = gmtime(&t);
    out[0] = 0;
    if (tp && fmt && wcsftime(out, (size_t)n, fmt, tp) == 0) out[0] = 0;
    const wchar_t *f = fmt ? fmt : L"";
    while (*f == L' ' || *f == L'\t') f++;
    if (f[0] == L'%' && f[1] == L'I' && out[0] == L'0') memmove(out, out + 1, (wcslen(out)) * sizeof(wchar_t));
    free(fmt);
}

/* ------------------------------------------------------------- paths */
int path_exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
int path_isdir(const wchar_t *p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static int is_abs(const wchar_t *p) {
    return (p[0] && p[1] == L':') || (p[0] == L'\\' && p[1] == L'\\');
}

wchar_t *find_exe(const wchar_t *name, wchar_t *out, int n) {
    lstrcpynW(out, name, n);
    if (is_abs(name) || path_exists(name)) return out;
    wchar_t root[MAX_PATH], cand[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"SystemRoot", root, MAX_PATH);
    if (!len || len >= MAX_PATH) lstrcpyW(root, L"C:\\windows");
    const wchar_t *sub[] = { L"", L"\\system32", L"\\syswow64" };
    for (int i = 0; i < 3; i++) {
        _snwprintf(cand, MAX_PATH, L"%ls%ls\\%ls", root, sub[i], name);
        if (path_exists(cand)) { lstrcpynW(out, cand, n); return out; }
    }
    return out;
}

void app_key(const wchar_t *path, wchar_t *out, int n) {
    const wchar_t *b = wcsrchr(path, L'\\'); const wchar_t *b2 = wcsrchr(path, L'/');
    if (b2 && (!b || b2 > b)) b = b2;
    lstrcpynW(out, b ? b + 1 : path, n);
    for (wchar_t *p = out; *p; p++) *p = (wchar_t)towlower(*p);
}

/* Parse a .lnk file by hand (same logic as the Python version; no COM needed). */
int lnk_info(const wchar_t *path, wchar_t *target, wchar_t *icon, int *index, int n) {
    if (target) target[0] = 0;
    if (icon) icon[0] = 0;
    if (index) *index = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    unsigned char *d = (unsigned char *)malloc(65536); DWORD got = 0;
    if (!d) { CloseHandle(f); return 0; }
    ReadFile(f, d, 65536, &got, NULL); CloseHandle(f);
    int ok = 0;
    #define RD32(o) ((o) + 4 <= got ? (DWORD)(d[o] | d[(o)+1] << 8 | d[(o)+2] << 16 | (DWORD)d[(o)+3] << 24) : 0)
    #define RD16(o) ((o) + 2 <= got ? (unsigned)(d[o] | d[(o)+1] << 8) : 0)
    if (got >= 76 && RD32(0) == 0x4C) {
        DWORD flags = RD32(0x14); size_t pos = 76;
        if (index) *index = (int)RD32(0x38);
        if (flags & 1) pos += 2 + RD16(pos);
        if (flags & 2) {
            size_t base = pos; DWORD size = RD32(base), lf = RD32(base + 8);
            DWORD local_off = RD32(base + 16), suffix_off = RD32(base + 24);
            if ((lf & 1) && target && base + local_off < got) {
                char ansi[MAX_PATH * 2] = { 0 };
                lstrcpynA(ansi, (const char *)d + base + local_off, MAX_PATH);
                if (suffix_off && base + suffix_off < got) {
                    size_t l = strlen(ansi);
                    lstrcpynA(ansi + l, (const char *)d + base + suffix_off, (int)(MAX_PATH * 2 - l));
                }
                MultiByteToWideChar(CP_ACP, 0, ansi, -1, target, n);
                ok = target[0] != 0;
            }
            pos = base + size;
        }
        int uni = (flags & 0x80) != 0;
        static const DWORD bits[] = { 0x4, 0x8, 0x10, 0x20, 0x40 };
        for (int i = 0; i < 5; i++) {
            if (!(flags & bits[i])) continue;
            unsigned cnt = RD16(pos); pos += 2;
            if (pos + (uni ? cnt * 2 : cnt) > got) break;
            if (bits[i] == 0x40 && icon) {
                wchar_t tmp[MAX_PATH] = { 0 };
                if (uni) { memcpy(tmp, d + pos, (cnt < MAX_PATH - 1 ? cnt : MAX_PATH - 1) * 2); }
                else {
                    char a[MAX_PATH] = { 0 }; memcpy(a, d + pos, cnt < MAX_PATH - 1 ? cnt : MAX_PATH - 1);
                    MultiByteToWideChar(CP_ACP, 0, a, -1, tmp, MAX_PATH);
                }
                ExpandEnvironmentStringsW(tmp, icon, (DWORD)n);
            }
            pos += uni ? cnt * 2 : cnt;
        }
    }
    free(d);
    return ok;
}

void exe_for(const wchar_t *path, wchar_t *out, int n) {
    size_t l = wcslen(path);
    if (l > 4 && _wcsicmp(path + l - 4, L".lnk") == 0) {
        wchar_t t[MAX_PATH];
        if (lnk_info(path, t, NULL, NULL, MAX_PATH) && path_exists(t)) { lstrcpynW(out, t, n); return; }
    }
    find_exe(path, out, n);
}

void display_name(const wchar_t *path, wchar_t *out, int n) {
    if (!wcscmp(path, L"::thispc")) { lstrcpynW(out, L"This PC", n); return; }
    wchar_t tmp[MAX_PATH]; lstrcpynW(tmp, path, MAX_PATH);
    size_t l = wcslen(tmp);
    while (l && (tmp[l - 1] == L'\\' || tmp[l - 1] == L'/')) tmp[--l] = 0;
    const wchar_t *b = wcsrchr(tmp, L'\\'); const wchar_t *b2 = wcsrchr(tmp, L'/');
    if (b2 && (!b || b2 > b)) b = b2;
    lstrcpynW(out, b ? b + 1 : tmp, n);
    if (!out[0]) lstrcpynW(out, tmp, n);
    l = wcslen(out);
    if (l > 4 && (!_wcsicmp(out + l - 4, L".exe") || !_wcsicmp(out + l - 4, L".lnk"))) out[l - 4] = 0;
    int has_lower = 0, has_upper = 0;
    for (wchar_t *p = out; *p; p++) {
        if (*p == L'_') *p = L' ';
        if (iswlower(*p)) has_lower = 1;
        if (iswupper(*p)) has_upper = 1;
    }
    if (has_lower && !has_upper) {              /* Python: name.title() if name.islower() */
        int start = 1;
        for (wchar_t *p = out; *p; p++) {
            if (iswalpha(*p)) { if (start) *p = (wchar_t)towupper(*p); start = 0; }
            else start = 1;
        }
    }
}

void window_process_path(HWND h, wchar_t *out, int n) {
    out[0] = 0; DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (!pid) return;
    HANDLE p = OpenProcess(0x1000 /* PROCESS_QUERY_LIMITED_INFORMATION */, FALSE, pid);
    if (!p) return;
    DWORD size = (DWORD)n;
    if (!QueryFullProcessImageNameW(p, 0, out, &size)) out[0] = 0;
    CloseHandle(p);
}

COLORREF c_hsv(double h, double s, double v) {
    h = h - 360.0 * (int)(h / 360.0); if (h < 0) h += 360.0;
    double c = v * s, hp = h / 60.0, m = v - c, hh = hp - 2.0 * (int)(hp / 2.0), x = c * (1.0 - (hh > 1.0 ? hh - 1.0 : 1.0 - hh));
    double r = 0, g = 0, b = 0; int k = (int)hp % 6;
    if (k == 0) { r = c; g = x; } else if (k == 1) { r = x; g = c; } else if (k == 2) { g = c; b = x; }
    else if (k == 3) { g = x; b = c; } else if (k == 4) { r = x; b = c; } else { r = c; b = x; }
    return RGB((int)((r + m) * 255 + 0.5), (int)((g + m) * 255 + 0.5), (int)((b + m) * 255 + 0.5));
}

void clock_tm(struct tm *out) {
    time_t t = time(NULL) + (time_t)cfg_num("clock_offset") + zone_offset_s(); struct tm *tp = gmtime(&t);
    if (tp) *out = *tp; else memset(out, 0, sizeof *out);
}
