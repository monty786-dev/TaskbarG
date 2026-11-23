#include "app.h"
#include <string.h>
#include <stdlib.h>

#define MAX_CACHE 256
typedef struct { wchar_t path[MAX_PATH]; HICON ic; } FileIcon;
static FileIcon g_files[MAX_CACHE]; static int g_nfiles;
static HICON g_owned[1024]; static int g_nowned;

typedef UINT (WINAPI *PrivateExtractIconsW_t)(LPCWSTR, int, int, int, HICON *, UINT *, UINT, UINT);

static void own(HICON ic) { if (ic && g_nowned < 1024) g_owned[g_nowned++] = ic; }

static HICON extract(const wchar_t *src, int index, int size) {
    static PrivateExtractIconsW_t pe; static int tried;
    if (!tried) { tried = 1; HMODULE u = GetModuleHandleW(L"user32.dll"); if (u) pe = (PrivateExtractIconsW_t)GetProcAddress(u, "PrivateExtractIconsW"); }
    if (pe) {
        HICON hic = NULL; UINT iid = 0;
        UINT n = pe(src, index, size, size, &hic, &iid, 1, 0);
        if (n && n != 0xFFFFFFFFu && hic) { own(hic); return hic; }
    }
    HICON big = NULL, small = NULL;
    if (ExtractIconExW(src, index, &big, &small, 1)) {
        own(big); own(small);
        return size > 32 ? (big ? big : small) : (small ? small : big);
    }
    return NULL;
}

static int ends_with(const wchar_t *s, const wchar_t *suf) {
    size_t a = wcslen(s), b = wcslen(suf);
    return a >= b && _wcsicmp(s + a - b, suf) == 0;
}

static HICON icon_from_source(const wchar_t *src, int idx) {
    if (ends_with(src, L".ico")) {
        HICON ic = (HICON)LoadImageW(NULL, src, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        if (ic) { own(ic); return ic; }
    }
    return extract(src, idx, 32);
}

static HICON lnk_icon(const wchar_t *path) {
    wchar_t target[MAX_PATH], icon[MAX_PATH]; int idx = 0;
    lnk_info(path, target, icon, &idx, MAX_PATH);
    const wchar_t *cands[2] = { icon, target }; int idxs[2] = { idx, 0 };
    for (int i = 0; i < 2; i++) {
        wchar_t src[MAX_PATH];
        if (!cands[i][0]) continue;
        ExpandEnvironmentStringsW(cands[i], src, MAX_PATH);
        if (path_exists(src) && !path_isdir(src)) {
            HICON ic = icon_from_source(src, idxs[i]);
            if (ic) return ic;
        }
    }
    return NULL;
}

HICON icon_for_file(const wchar_t *path) {
    if (!wcsncmp(path, L"winshell:", 9)) return LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(100));
    if (!wcsncmp(path, L"fake:", 5)) { JNode *f = fake_app(path); JNode *v = f ? json_get(f, "icon") : NULL; HICON k = NULL;
        if (v && v->s && v->s[0]) { wchar_t w[MAX_PATH]; w_from_utf8_buf(v->s, w, MAX_PATH); k = img_to_icon(w, 48); } return k ? k : LoadIconW(NULL, IDI_APPLICATION); }
    for (int i = 0; i < g_nfiles; i++) if (!_wcsicmp(g_files[i].path, path)) return g_files[i].ic;
    HICON icon = NULL;
    if (ends_with(path, L".lnk")) icon = lnk_icon(path);
    else if (ends_with(path, L".exe") || ends_with(path, L".ico") || ends_with(path, L".dll")) {
        wchar_t full[MAX_PATH]; find_exe(path, full, MAX_PATH);
        icon = extract(full, 0, 32);
    }
    if (!icon) {
        SHFILEINFOW sfi; memset(&sfi, 0, sizeof sfi);
        if (SHGetFileInfoW(path, 0, &sfi, sizeof sfi, SHGFI_ICON | SHGFI_SMALLICON) && sfi.hIcon) { icon = sfi.hIcon; own(icon); }
    }
    if (!icon) icon = LoadIconW(NULL, IDI_APPLICATION);
    if (g_nfiles >= MAX_CACHE) g_nfiles = 0;
    lstrcpynW(g_files[g_nfiles].path, path, MAX_PATH); g_files[g_nfiles++].ic = icon;
    return icon;
}

HICON icon_for_window(HWND h, const wchar_t *exe) {
    static const int kinds[3] = { ICON_SMALL2, ICON_SMALL, ICON_BIG };
    for (int i = 0; i < 3; i++) {
        DWORD_PTR res = 0;
        if (SendMessageTimeoutW(h, WM_GETICON, (WPARAM)kinds[i], 0, SMTO_ABORTIFHUNG, 100, &res) && res) return (HICON)res;
    }
    HICON ic = (HICON)GetClassLongPtrW(h, GCLP_HICONSM); if (ic) return ic;
    ic = (HICON)GetClassLongPtrW(h, GCLP_HICON); if (ic) return ic;
    return exe && exe[0] ? icon_for_file(exe) : LoadIconW(NULL, IDI_APPLICATION);
}

COLORREF icon_accent(HICON ic, COLORREF def) {
    ICONINFO ii; BITMAP bm;
    if (!ic || !GetIconInfo(ic, &ii)) return def;
    COLORREF result = def; unsigned char *buf = NULL;
    if (!ii.hbmColor || !GetObjectW(ii.hbmColor, sizeof bm, &bm)) goto done;
    int w = bm.bmWidth, h = bm.bmHeight;
    if (w <= 0 || h <= 0 || w > 256 || h > 256) goto done;
    BITMAPINFO bi; memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    buf = (unsigned char *)malloc((size_t)w * h * 4);
    if (!buf) goto done;
    HDC dc = GetDC(NULL); int lines = GetDIBits(dc, ii.hbmColor, 0, h, buf, &bi, DIB_RGB_COLORS); ReleaseDC(NULL, dc);
    if (lines != h) goto done;
    int has_alpha = 0; size_t total = (size_t)w * h * 4;
    for (size_t i = 3; i < total; i += 4) if (buf[i]) { has_alpha = 1; break; }
    long long rs = 0, gs = 0, bs = 0, ws = 0;
    for (size_t i = 0; i < total; i += 4) {
        int b = buf[i], g = buf[i + 1], r = buf[i + 2], a = buf[i + 3];
        if (has_alpha && a < 128) continue;
        int mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
        int wgt = mx < 24 ? 1 : (mx - mn) + 8;
        rs += (long long)r * wgt; gs += (long long)g * wgt; bs += (long long)b * wgt; ws += wgt;
    }
    if (!ws) goto done;
    int r = (int)(rs / ws), g = (int)(gs / ws), b = (int)(bs / ws);
    int lum = (r * 299 + g * 587 + b * 114) / 1000;
    if (lum > 0 && lum < 110) {
        double f = 110.0 / lum;
        r = r * f > 255 ? 255 : (int)(r * f); g = g * f > 255 ? 255 : (int)(g * f); b = b * f > 255 ? 255 : (int)(b * f);
    }
    result = RGB(r, g, b);
done:
    free(buf);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return result;
}

void icons_close(void) {
    for (int i = 0; i < g_nowned; i++) DestroyIcon(g_owned[i]);
    g_nowned = 0; g_nfiles = 0;
}
