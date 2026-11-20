#include "json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static char *dup_str(const char *s) {
    size_t n = strlen(s) + 1; char *d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

JNode *json_new(JType t) {
    JNode *n = (JNode *)calloc(1, sizeof(JNode));
    if (n) n->t = t;
    return n;
}
JNode *json_str(const char *s) { JNode *n = json_new(J_STR); if (n) n->s = dup_str(s ? s : ""); return n; }
JNode *json_num(double v, int is_int) { JNode *n = json_new(J_NUM); if (n) { n->n = v; n->is_int = is_int; } return n; }
JNode *json_bool(int b) { JNode *n = json_new(J_BOOL); if (n) n->b = b ? 1 : 0; return n; }

void json_free(JNode *n) {
    while (n) {
        JNode *next = n->next;
        json_free(n->child);
        free(n->key); free(n->s); free(n);
        n = next;
    }
}

JNode *json_clone(const JNode *n) {
    if (!n) return NULL;
    JNode *c = json_new(n->t);
    if (!c) return NULL;
    c->n = n->n; c->b = n->b; c->is_int = n->is_int;
    if (n->s) c->s = dup_str(n->s);
    JNode **tail = &c->child;
    for (const JNode *k = n->child; k; k = k->next) {
        JNode *kc = json_clone(k);
        if (!kc) break;
        kc->key = k->key ? dup_str(k->key) : NULL;
        *tail = kc; tail = &kc->next;
    }
    return c;
}

JNode *json_get(const JNode *obj, const char *key) {
    if (!obj || obj->t != J_OBJ) return NULL;
    for (JNode *c = obj->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0) return c;
    return NULL;
}

JNode *json_put(JNode *obj, const char *key, JNode *child) {
    if (!obj || !child) { json_free(child); return NULL; }
    free(child->key); child->key = dup_str(key);
    JNode *prev = NULL;
    for (JNode *c = obj->child; c; prev = c, c = c->next) {
        if (c->key && strcmp(c->key, key) == 0) {
            child->next = c->next;
            if (prev) prev->next = child; else obj->child = child;
            c->next = NULL; json_free(c);
            return child;
        }
    }
    child->next = NULL;
    if (prev) prev->next = child; else obj->child = child;
    return child;
}

JNode *json_push(JNode *arr, JNode *child) {
    if (!arr || !child) { json_free(child); return NULL; }
    child->next = NULL;
    if (!arr->child) arr->child = child;
    else { JNode *c = arr->child; while (c->next) c = c->next; c->next = child; }
    return child;
}

void json_remove(JNode *obj, const char *key) {
    if (!obj) return;
    JNode *prev = NULL;
    for (JNode *c = obj->child; c; prev = c, c = c->next) {
        if (c->key && strcmp(c->key, key) == 0) {
            if (prev) prev->next = c->next; else obj->child = c->next;
            c->next = NULL; json_free(c); return;
        }
    }
}

int json_count(const JNode *n) {
    int k = 0;
    if (n) for (const JNode *c = n->child; c; c = c->next) k++;
    return k;
}

/* ------------------------------------------------------------------ parse */
typedef struct { const char *p; int err; } Parser;

static void skip_ws(Parser *ps) {
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') ps->p++;
}

static int hex4(const char *p) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i]; v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}

static int put_utf8(char *o, unsigned cp) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | (cp >> 6)); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) {
        o[0] = (char)(0xE0 | (cp >> 12)); o[1] = (char)(0x80 | ((cp >> 6) & 63));
        o[2] = (char)(0x80 | (cp & 63)); return 3;
    }
    o[0] = (char)(0xF0 | (cp >> 18)); o[1] = (char)(0x80 | ((cp >> 12) & 63));
    o[2] = (char)(0x80 | ((cp >> 6) & 63)); o[3] = (char)(0x80 | (cp & 63)); return 4;
}

static char *parse_string(Parser *ps) {
    if (*ps->p != '"') { ps->err = 1; return NULL; }
    ps->p++;
    size_t cap = 32, len = 0; char *out = (char *)malloc(cap);
    if (!out) { ps->err = 1; return NULL; }
    while (*ps->p && *ps->p != '"') {
        if (len + 8 >= cap) { cap *= 2; char *t = (char *)realloc(out, cap); if (!t) { free(out); ps->err = 1; return NULL; } out = t; }
        unsigned char c = (unsigned char)*ps->p++;
        if (c != '\\') { out[len++] = (char)c; continue; }
        char e = *ps->p++;
        switch (e) {
            case 'n': out[len++] = '\n'; break;
            case 't': out[len++] = '\t'; break;
            case 'r': out[len++] = '\r'; break;
            case 'b': out[len++] = '\b'; break;
            case 'f': out[len++] = '\f'; break;
            case '/': out[len++] = '/'; break;
            case '\\': out[len++] = '\\'; break;
            case '"': out[len++] = '"'; break;
            case 'u': {
                int v = hex4(ps->p);
                if (v < 0) { free(out); ps->err = 1; return NULL; }
                ps->p += 4; unsigned cp = (unsigned)v;
                if (cp >= 0xD800 && cp < 0xDC00 && ps->p[0] == '\\' && ps->p[1] == 'u') {
                    int lo = hex4(ps->p + 2);
                    if (lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + ((unsigned)lo - 0xDC00); ps->p += 6; }
                }
                len += (size_t)put_utf8(out + len, cp); break;
            }
            default: free(out); ps->err = 1; return NULL;
        }
    }
    if (*ps->p != '"') { free(out); ps->err = 1; return NULL; }
    ps->p++; out[len] = 0; return out;
}

static JNode *parse_value(Parser *ps, int depth) {
    if (depth > 64) { ps->err = 1; return NULL; }
    skip_ws(ps);
    char c = *ps->p;
    if (c == '{') {
        JNode *o = json_new(J_OBJ); ps->p++; skip_ws(ps);
        if (*ps->p == '}') { ps->p++; return o; }
        for (;;) {
            skip_ws(ps); char *k = parse_string(ps);
            if (!k) { json_free(o); return NULL; }
            skip_ws(ps);
            if (*ps->p != ':') { free(k); json_free(o); ps->err = 1; return NULL; }
            ps->p++;
            JNode *v = parse_value(ps, depth + 1);
            if (!v) { free(k); json_free(o); return NULL; }
            json_put(o, k, v); free(k); skip_ws(ps);
            if (*ps->p == ',') { ps->p++; continue; }
            if (*ps->p == '}') { ps->p++; return o; }
            json_free(o); ps->err = 1; return NULL;
        }
    }
    if (c == '[') {
        JNode *a = json_new(J_ARR); ps->p++; skip_ws(ps);
        if (*ps->p == ']') { ps->p++; return a; }
        for (;;) {
            JNode *v = parse_value(ps, depth + 1);
            if (!v) { json_free(a); return NULL; }
            json_push(a, v); skip_ws(ps);
            if (*ps->p == ',') { ps->p++; continue; }
            if (*ps->p == ']') { ps->p++; return a; }
            json_free(a); ps->err = 1; return NULL;
        }
    }
    if (c == '"') { char *s = parse_string(ps); if (!s) return NULL; JNode *n = json_new(J_STR); n->s = s; return n; }
    if (!strncmp(ps->p, "true", 4)) { ps->p += 4; return json_bool(1); }
    if (!strncmp(ps->p, "false", 5)) { ps->p += 5; return json_bool(0); }
    if (!strncmp(ps->p, "null", 4)) { ps->p += 4; return json_new(J_NULL); }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char *end; double v = strtod(ps->p, &end);
        if (end == ps->p) { ps->err = 1; return NULL; }
        int is_int = 1;
        for (const char *q = ps->p; q < end; q++) if (*q == '.' || *q == 'e' || *q == 'E') is_int = 0;
        ps->p = end; return json_num(v, is_int);
    }
    ps->err = 1; return NULL;
}

JNode *json_parse(const char *text) {
    if (!text) return NULL;
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text += 3;
    Parser ps = { text, 0 };
    JNode *v = parse_value(&ps, 0);
    return ps.err ? NULL : v;
}

/* ------------------------------------------------------------------- dump */
typedef struct { char *d; size_t n, cap; } Buf;

static void b_put(Buf *b, const char *s, size_t len) {
    if (b->n + len + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        while (nc < b->n + len + 1) nc *= 2;
        char *t = (char *)realloc(b->d, nc); if (!t) return;
        b->d = t; b->cap = nc;
    }
    memcpy(b->d + b->n, s, len); b->n += len; b->d[b->n] = 0;
}
static void b_str(Buf *b, const char *s) { b_put(b, s, strlen(s)); }
static void b_indent(Buf *b, int lvl) { for (int i = 0; i < lvl * 2; i++) b_put(b, " ", 1); }

static void dump_string(Buf *b, const char *s) {
    char tmp[16]; b_put(b, "\"", 1);
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned c = *p;
        if (c == '"') { b_str(b, "\\\""); p++; }
        else if (c == '\\') { b_str(b, "\\\\"); p++; }
        else if (c == '\n') { b_str(b, "\\n"); p++; }
        else if (c == '\r') { b_str(b, "\\r"); p++; }
        else if (c == '\t') { b_str(b, "\\t"); p++; }
        else if (c == '\b') { b_str(b, "\\b"); p++; }
        else if (c == '\f') { b_str(b, "\\f"); p++; }
        else if (c < 0x20) { snprintf(tmp, sizeof tmp, "\\u%04x", c); b_str(b, tmp); p++; }
        else if (c < 0x80) { b_put(b, (const char *)p, 1); p++; }
        else {
            unsigned cp = 0; int extra = 0;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
            else { p++; continue; }
            p++;
            for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++, p++) cp = (cp << 6) | (*p & 0x3F);
            if (cp >= 0x10000) {
                cp -= 0x10000;
                snprintf(tmp, sizeof tmp, "\\u%04x\\u%04x", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            } else snprintf(tmp, sizeof tmp, "\\u%04x", cp);
            b_str(b, tmp);
        }
    }
    b_put(b, "\"", 1);
}

static void dump_value(Buf *b, const JNode *n, int lvl) {
    char tmp[64];
    switch (n->t) {
        case J_NULL: b_str(b, "null"); break;
        case J_BOOL: b_str(b, n->b ? "true" : "false"); break;
        case J_NUM:
            if (n->is_int && fabs(n->n) < 9e15) snprintf(tmp, sizeof tmp, "%.0f", n->n);
            else {
                snprintf(tmp, sizeof tmp, "%.15g", n->n);
                if (!strpbrk(tmp, ".eEn")) strcat(tmp, ".0");
            }
            b_str(b, tmp); break;
        case J_STR: dump_string(b, n->s ? n->s : ""); break;
        case J_ARR:
        case J_OBJ: {
            int obj = n->t == J_OBJ;
            if (!n->child) { b_str(b, obj ? "{}" : "[]"); break; }
            b_str(b, obj ? "{\n" : "[\n");
            for (const JNode *c = n->child; c; c = c->next) {
                b_indent(b, lvl + 1);
                if (obj) { dump_string(b, c->key ? c->key : ""); b_str(b, ": "); }
                dump_value(b, c, lvl + 1);
                b_str(b, c->next ? ",\n" : "\n");
            }
            b_indent(b, lvl); b_str(b, obj ? "}" : "]"); break;
        }
    }
}

char *json_dump(const JNode *n) {
    Buf b = { 0, 0, 0 };
    if (!n) return NULL;
    dump_value(&b, n, 0);
    return b.d;
}
