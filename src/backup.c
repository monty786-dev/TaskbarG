/* Backup / restore: one .wsbk file with config.json, every image and icon the settings point to,
 * shortcuts (.lnk / .url) used by the app lists, and the Wine / user Desktop shortcuts. */
#include "app.h"
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PK 3000
typedef struct { char name[320]; wchar_t path[MAX_PATH]; } PItem;
static PItem *g_pk; static int g_npk;
static const char MAGIC[] = "WSBK1\n";

static int is_abs_w(const wchar_t *p) { return (p[0] && p[1] == L':') || (p[0] == L'\\' && p[1] == L'\\'); }
static const char *base8(const char *p) { const char *a = strrchr(p, '\\'), *b = strrchr(p, '/'); if (b && (!a || b > a)) a = b; return a ? a + 1 : p; }

static int add_file(const char *prefix, const wchar_t *path, char *entry_out) {
    char *u = utf8_from_w(path);
    for (int i = 0; i < g_npk; i++) if (!_wcsicmp(g_pk[i].path, path)) { lstrcpynA(entry_out, g_pk[i].name, 320); free(u); return 1; }
    if (g_npk >= MAX_PK) { free(u); return 0; }
    snprintf(g_pk[g_npk].name, 320, "%s/%d_%s", prefix, g_npk, base8(u)); lstrcpynW(g_pk[g_npk].path, path, MAX_PATH);
    lstrcpynA(entry_out, g_pk[g_npk].name, 320); g_npk++; free(u); return 1;
}
static int has_ext(const char *s, const char *list) {                    /* list: ".png.jpg" */
    const char *dot = strrchr(s, '.'); if (!dot || strlen(dot) > 6) return 0; char e[8]; lstrcpynA(e, dot, 8); for (char *c = e; *c; c++) *c = (char)tolower((unsigned char)*c);
    return strstr(list, e) != NULL;
}
static void pack_strings(JNode *n) {
    for (JNode *c = n->child; c; c = c->next) {
        if (c->t == J_STR && c->s) {
            int img = has_ext(c->s, ".png.jpg.jpeg.bmp.gif.ico"), lnk = has_ext(c->s, ".lnk.url"); if (!img && !lnk) continue;
            wchar_t w[MAX_PATH]; w_from_utf8_buf(c->s, w, MAX_PATH); if (!is_abs_w(w) || !path_exists(w) || path_isdir(w)) continue;
            char entry[320]; if (!add_file(img ? "assets" : "shortcuts", w, entry)) continue;
            char *ns = (char *)malloc(strlen(entry) + 8); if (!ns) continue; snprintf(ns, strlen(entry) + 8, "@wsbk:%s", entry); free(c->s); c->s = ns;
        } else if (c->child) pack_strings(c);
    }
}
static void scan_desktop(const wchar_t *dir) {
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; _snwprintf(pat, MAX_PATH, L"%ls\\*", dir); HANDLE h = FindFirstFileW(pat, &fd); if (h == INVALID_HANDLE_VALUE) return;
    do { if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue; char *u = utf8_from_w(fd.cFileName); if (has_ext(u, ".lnk.url") && g_npk < MAX_PK) {
            snprintf(g_pk[g_npk].name, 320, "desktop/%s", u); _snwprintf(g_pk[g_npk].path, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName); g_npk++; } free(u);
    } while (FindNextFileW(h, &fd)); FindClose(h);
}
static void put_entry(FILE *f, const char *name, const void *data, unsigned size) {
    unsigned nl = (unsigned)strlen(name); fwrite(&nl, 4, 1, f); fwrite(name, 1, nl, f); fwrite(&size, 4, 1, f); if (size) fwrite(data, 1, size, f);
}
static int pick_path(HWND owner, int save, wchar_t *out) {
    OPENFILENAMEW o; memset(&o, 0, sizeof o); out[0] = 0; if (save) lstrcpyW(out, L"WinShell-backup.wsbk");
    o.lStructSize = sizeof o; o.hwndOwner = owner; o.lpstrFile = out; o.nMaxFile = MAX_PATH; o.lpstrFilter = L"WinShell backup (*.wsbk)\0*.wsbk\0All files\0*.*\0"; o.lpstrDefExt = L"wsbk";
    o.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR) : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR);
    g_menu_depth++; BOOL ok = save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o); g_menu_depth--; return ok;
}

void app_backup_save(HWND owner) {
    wchar_t out[MAX_PATH]; if (!pick_path(owner, 1, out)) return;
    g_pk = (PItem *)calloc(MAX_PK, sizeof(PItem)); if (!g_pk) return; g_npk = 0;
    JNode *copy = json_clone(g_cfg); if (!copy) { free(g_pk); return; }
    pack_strings(copy);
    wchar_t d[MAX_PATH], base[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"USERPROFILE", base, MAX_PATH); if (n && n < MAX_PATH) { _snwprintf(d, MAX_PATH, L"%ls\\Desktop", base); scan_desktop(d); }
    n = GetEnvironmentVariableW(L"PUBLIC", base, MAX_PATH); if (!n || n >= MAX_PATH) lstrcpyW(base, L"C:\\users\\Public"); _snwprintf(d, MAX_PATH, L"%ls\\Desktop", base); scan_desktop(d);
    char *json = json_dump(copy); json_free(copy); FILE *f = _wfopen(out, L"wb"); int files = 0;
    if (f && json) {
        fwrite(MAGIC, 1, 6, f); put_entry(f, "config.json", json, (unsigned)strlen(json));
        for (int i = 0; i < g_npk; i++) { FILE *in = _wfopen(g_pk[i].path, L"rb"); if (!in) continue; fseek(in, 0, SEEK_END); long sz = ftell(in); fseek(in, 0, SEEK_SET);
            if (sz < 0 || sz > 64 * 1024 * 1024) { fclose(in); continue; } unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1); if (buf) { size_t got = fread(buf, 1, (size_t)sz, in); put_entry(f, g_pk[i].name, buf, (unsigned)got); free(buf); files++; } fclose(in); }
        fclose(f); wchar_t msg[160]; _snwprintf(msg, 160, L"Backup saved: settings plus %d image / shortcut files.", files); MessageBoxW(owner, msg, L"WinShell backup", MB_OKCANCEL | MB_TOPMOST);
    } else MessageBoxW(owner, L"Could not write the backup file.", L"WinShell backup", MB_ICONERROR);
    free(json); free(g_pk); g_pk = NULL; g_npk = 0;
}

static void make_dirs(const wchar_t *dir) { wchar_t t[MAX_PATH]; lstrcpynW(t, dir, MAX_PATH); for (wchar_t *p = t + 3; *p; p++) if (*p == L'\\') { *p = 0; CreateDirectoryW(t, NULL); *p = L'\\'; } CreateDirectoryW(t, NULL); }
static void unpack_strings(JNode *n, const wchar_t *root) {
    for (JNode *c = n->child; c; c = c->next) {
        if (c->t == J_STR && c->s && !strncmp(c->s, "@wsbk:", 6)) {
            const char *e = c->s + 6; if (!strncmp(e, "desktop/", 8)) continue; wchar_t dest[MAX_PATH], we[320]; w_from_utf8_buf(e, we, 320); for (wchar_t *p = we; *p; p++) if (*p == L'/') *p = L'\\';
            _snwprintf(dest, MAX_PATH, L"%ls\\%ls", root, we); char *u = utf8_from_w(dest); free(c->s); c->s = u;
        } else if (c->child) unpack_strings(c, root);
    }
}
void app_backup_restore(HWND owner) {
    wchar_t in[MAX_PATH]; if (!pick_path(owner, 0, in)) return;
    if (!app_confirm(owner, L"Restore backup", L"Restoring replaces your current settings with the backup. Continue?")) return;
    FILE *f = _wfopen(in, L"rb"); char mg[6]; if (!f || fread(mg, 1, 6, f) != 6 || memcmp(mg, MAGIC, 6) != 0) { if (f) fclose(f); MessageBoxW(owner, L"This is not a WinShell backup file.", L"WinShell backup", MB_ICONERROR); return; }
    wchar_t root[MAX_PATH]; _snwprintf(root, MAX_PATH, L"%ls\\restored", g_config_dir); make_dirs(root); char *json = NULL; int files = 0;
    for (;;) {
        unsigned nl = 0, sz = 0; if (fread(&nl, 4, 1, f) != 1 || nl == 0 || nl > 300) break; char name[320]; if (fread(name, 1, nl, f) != nl) break; name[nl] = 0; if (fread(&sz, 4, 1, f) != 1) break;
        unsigned char *buf = (unsigned char *)malloc((size_t)sz + 1); if (!buf || fread(buf, 1, sz, f) != sz) { free(buf); break; } buf[sz] = 0;
        if (strstr(name, "..")) { free(buf); continue; }
        if (!strcmp(name, "config.json")) json = (char *)buf;
        else {
            wchar_t dest[MAX_PATH], we[320]; w_from_utf8_buf(name, we, 320); for (wchar_t *p = we; *p; p++) if (*p == L'/') *p = L'\\';
            if (!wcsncmp(we, L"desktop\\", 8)) { wchar_t base[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"USERPROFILE", base, MAX_PATH); if (n && n < MAX_PATH) _snwprintf(dest, MAX_PATH, L"%ls\\Desktop\\%ls", base, we + 8); else { free(buf); continue; } }
            else _snwprintf(dest, MAX_PATH, L"%ls\\%ls", root, we);
            wchar_t dir[MAX_PATH]; lstrcpynW(dir, dest, MAX_PATH); wchar_t *sl = wcsrchr(dir, L'\\'); if (sl) { *sl = 0; make_dirs(dir); }
            FILE *o = _wfopen(dest, L"wb"); if (o) { fwrite(buf, 1, sz, o); fclose(o); files++; } free(buf);
        }
    }
    fclose(f);
    if (json) { JNode *cfg = json_parse(json); if (cfg) { unpack_strings(cfg, root); char *out = json_dump(cfg); if (out) { FILE *o = _wfopen(g_config_path, L"wb"); if (o) { fwrite(out, 1, strlen(out), o); fclose(o); } free(out); } json_free(cfg);
            app_reload(); wchar_t msg[160]; _snwprintf(msg, 160, L"Backup restored (%d files).", files); MessageBoxW(owner, msg, L"WinShell backup", MB_OKCANCEL | MB_TOPMOST); } free(json); }
    else MessageBoxW(owner, L"The backup has no settings.", L"WinShell backup", MB_ICONERROR);
}
