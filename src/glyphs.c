/* Vector icons drawn with GDI in any colour. Used for every built-in button icon. */
#include "app.h"
#include <math.h>

static int rw_(const RECT *r) { return r->right - r->left; }
static int rh_(const RECT *r) { return r->bottom - r->top; }
static RECT R4(int l, int t, int r, int b) { RECT x = { l, t, r, b }; return x; }

typedef struct { HDC dc; HPEN pen, op; HGDIOBJ ob; HBRUSH brush; HGDIOBJ obr; } Pen;
static void pen_on(Pen *p, HDC dc, COLORREF c, int w, int fill) {
    p->dc = dc; p->pen = CreatePen(PS_SOLID, w < 1 ? 1 : w, c);
    p->brush = fill ? CreateSolidBrush(c) : NULL;
    p->op = (HPEN)SelectObject(dc, p->pen);
    p->obr = SelectObject(dc, p->brush ? (HGDIOBJ)p->brush : GetStockObject(NULL_BRUSH));
    (void)p->ob;
}
static void pen_off(Pen *p) {
    SelectObject(p->dc, p->obr); SelectObject(p->dc, p->op);
    DeleteObject(p->pen); if (p->brush) DeleteObject(p->brush);
}
static void box(HDC dc, int l, int t, int r, int b, COLORREF c) {
    RECT x = { l, t, r, b }; g_fill(dc, x, c);
}

void glyph_draw(int g, HDC dc, RECT r, COLORREF c) {
    int s = rw_(&r) < rh_(&r) ? rw_(&r) : rh_(&r);
    if (s < 4) return;
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, h = s / 2;
    int lw = s / 11 < 1 ? 1 : s / 11; Pen p;
    switch (g) {
    case G_NINEBOX: {                                   /* 3 x 3 squares (default Start icon) */
        int gap = s / 12 < 1 ? 1 : s / 12, b = (s - 2 * gap) / 3; if (b < 2) b = 2;
        int tot = 3 * b + 2 * gap, x0 = cx - tot / 2, y0 = cy - tot / 2;
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++)
            box(dc, x0 + j * (b + gap), y0 + i * (b + gap), x0 + j * (b + gap) + b, y0 + i * (b + gap) + b, c);
        break; }
    case G_WINLOGO: {
        int gap = s / 12 < 1 ? 1 : s / 12, pane = (s - gap) / 2, x0 = cx - (2 * pane + gap) / 2, y0 = cy - (2 * pane + gap) / 2;
        for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++)
            box(dc, x0 + j * (pane + gap), y0 + i * (pane + gap), x0 + j * (pane + gap) + pane, y0 + i * (pane + gap) + pane, c);
        break; }
    case G_POWER:
        pen_on(&p, dc, c, lw + 1, 0);
        Arc(dc, cx - h + 2, cy - h + 3, cx + h - 2, cy + h + 1, cx + h / 2, cy - h + 3, cx - h / 2, cy - h + 3);
        MoveToEx(dc, cx, cy - h + 1, NULL); LineTo(dc, cx, cy - 1); pen_off(&p); break;
    case G_GEAR: {
        pen_on(&p, dc, c, lw, 0);
        int rr = h * 6 / 10; Ellipse(dc, cx - rr, cy - rr, cx + rr, cy + rr);
        Ellipse(dc, cx - rr / 2, cy - rr / 2, cx + rr / 2, cy + rr / 2);
        for (int i = 0; i < 8; i++) {
            double a = i * 3.14159265 / 4; int x1 = cx + (int)(cos(a) * rr), y1 = cy + (int)(sin(a) * rr);
            int x2 = cx + (int)(cos(a) * (h - 1)), y2 = cy + (int)(sin(a) * (h - 1));
            MoveToEx(dc, x1, y1, NULL); LineTo(dc, x2, y2);
        }
        pen_off(&p); break; }
    case G_RUN:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h + 1, cy - h * 7 / 10, cx + h - 1, cy + h * 7 / 10);
        MoveToEx(dc, cx - h / 2, cy - h / 4, NULL); LineTo(dc, cx - h / 8, cy); LineTo(dc, cx - h / 2, cy + h / 4);
        MoveToEx(dc, cx, cy + h / 4, NULL); LineTo(dc, cx + h / 2, cy + h / 4); pen_off(&p); break;
    case G_PROGRAMS:
        for (int i = -1; i <= 1; i++) { box(dc, cx - h + 1, cy + i * (h / 2) - lw / 2 - 1, cx - h + 1 + lw * 2, cy + i * (h / 2) + lw + 1, c);
            box(dc, cx - h + 1 + lw * 3, cy + i * (h / 2) - lw / 2, cx + h - 1, cy + i * (h / 2) + lw / 2 + 1, c); }
        break;
    case G_SYSTEM:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h + 1, cy - h * 7 / 10, cx + h - 1, cy + h / 4);
        MoveToEx(dc, cx, cy + h / 4, NULL); LineTo(dc, cx, cy + h * 6 / 10);
        MoveToEx(dc, cx - h / 2, cy + h * 6 / 10, NULL); LineTo(dc, cx + h / 2, cy + h * 6 / 10); pen_off(&p); break;
    case G_COLLAPSE:
        for (int i = -1; i <= 1; i++) box(dc, cx - h * 7 / 10, cy + i * (h / 2) - lw / 2, cx + h * 7 / 10, cy + i * (h / 2) + lw - lw / 2, c);
        break;
    case G_SEARCH:
        pen_on(&p, dc, c, lw + 1, 0); Ellipse(dc, cx - h + 2, cy - h + 2, cx + h / 8, cy + h / 8);
        MoveToEx(dc, cx - h / 8 + 2, cy - h / 8 + 2, NULL); LineTo(dc, cx + h - 2, cy + h - 2); pen_off(&p); break;
    case G_USER:
        pen_on(&p, dc, c, lw, 1); Ellipse(dc, cx - h / 3, cy - h * 7 / 10, cx + h / 3, cy - h / 10);
        pen_off(&p); pen_on(&p, dc, c, lw, 0); Arc(dc, cx - h * 7 / 10, cy + h / 8, cx + h * 7 / 10, cy + h * 13 / 10, cx + h * 7 / 10, cy + h * 7 / 10, cx - h * 7 / 10, cy + h * 7 / 10);
        pen_off(&p); break;
    case G_DESK:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h + 1, cy - h * 6 / 10, cx + h - 1, cy + h * 6 / 10); pen_off(&p); break;
    case G_PREVIEW:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h / 8, cy - h * 7 / 10, cx + h * 8 / 10, cy + h / 5);
        Rectangle(dc, cx - h * 8 / 10, cy - h / 5, cx + h / 8, cy + h * 7 / 10); pen_off(&p); break;
    case G_PLUS:
        box(dc, cx - h * 6 / 10, cy - lw / 2, cx + h * 6 / 10, cy + lw - lw / 2, c); box(dc, cx - lw / 2, cy - h * 6 / 10, cx + lw - lw / 2, cy + h * 6 / 10, c); break;
    case G_SWITCH:
        pen_on(&p, dc, c, lw, 0);
        MoveToEx(dc, cx - h + 2, cy - h / 4, NULL); LineTo(dc, cx + h - 2, cy - h / 4); LineTo(dc, cx + h / 3, cy - h * 6 / 10);
        MoveToEx(dc, cx + h - 2, cy + h / 4, NULL); LineTo(dc, cx - h + 2, cy + h / 4); LineTo(dc, cx - h / 3, cy + h * 6 / 10); pen_off(&p); break;
    case G_CHEVRON_R: case G_CHEVRON_D: {
        pen_on(&p, dc, c, lw, 0);
        if (g == G_CHEVRON_R) { MoveToEx(dc, cx - h / 4, cy - h / 2, NULL); LineTo(dc, cx + h / 4, cy); LineTo(dc, cx - h / 4, cy + h / 2); }
        else { MoveToEx(dc, cx - h / 2, cy - h / 4, NULL); LineTo(dc, cx, cy + h / 4); LineTo(dc, cx + h / 2, cy - h / 4); }
        pen_off(&p); break; }
    case G_FOLDER:
        pen_on(&p, dc, c, lw, 1); Rectangle(dc, cx - h + 1, cy - h / 2, cx + h - 1, cy + h * 7 / 10);
        box(dc, cx - h + 1, cy - h * 7 / 10, cx - h / 8, cy - h / 2 + 1, c); pen_off(&p); break;
    case G_DOC:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h * 6 / 10, cy - h + 1, cx + h * 6 / 10, cy + h - 1);
        for (int i = -1; i <= 1; i++) { MoveToEx(dc, cx - h / 3, cy + i * h / 3, NULL); LineTo(dc, cx + h / 3, cy + i * h / 3); } pen_off(&p); break;
    case G_PIC:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h + 1, cy - h * 7 / 10, cx + h - 1, cy + h * 7 / 10);
        MoveToEx(dc, cx - h + 2, cy + h / 2, NULL); LineTo(dc, cx - h / 4, cy - h / 5); LineTo(dc, cx + h / 6, cy + h / 5); LineTo(dc, cx + h / 2, cy - h / 8); LineTo(dc, cx + h - 2, cy + h / 2); pen_off(&p); break;
    case G_MUSIC:
        pen_on(&p, dc, c, lw, 1); Ellipse(dc, cx - h / 2, cy + h / 8, cx - h / 8, cy + h / 2 + 2); Ellipse(dc, cx + h / 6, cy + h / 8 - 3, cx + h / 2, cy + h / 2 - 1); pen_off(&p);
        pen_on(&p, dc, c, lw, 0); MoveToEx(dc, cx - h / 8, cy + h / 3, NULL); LineTo(dc, cx - h / 8, cy - h * 7 / 10); LineTo(dc, cx + h / 2, cy - h * 9 / 10); LineTo(dc, cx + h / 2, cy + h / 5); pen_off(&p); break;
    case G_PC: glyph_draw(G_SYSTEM, dc, r, c); break;
    case G_PANEL:
        for (int i = -1; i <= 1; i++) { box(dc, cx - h + 1, cy + i * (h / 2) - 1, cx + h - 1, cy + i * (h / 2) + 1, c); box(dc, cx + i * (h / 3) - 2, cy + i * (h / 2) - 3, cx + i * (h / 3) + 2, cy + i * (h / 2) + 3, c); }
        break;
    case G_HELP: {
        pen_on(&p, dc, c, lw, 0); Ellipse(dc, cx - h + 1, cy - h + 1, cx + h - 1, cy + h - 1);
        Arc(dc, cx - h / 3, cy - h / 2, cx + h / 3, cy + h / 8, cx + h / 3, cy - h / 6, cx, cy + h / 8);
        box(dc, cx - 1, cy + h / 3, cx + 1, cy + h / 3 + 2, c); pen_off(&p); break; }
    case G_CLOSE:
        pen_on(&p, dc, c, lw, 0); MoveToEx(dc, cx - h / 2, cy - h / 2, NULL); LineTo(dc, cx + h / 2 + 1, cy + h / 2 + 1);
        MoveToEx(dc, cx + h / 2, cy - h / 2, NULL); LineTo(dc, cx - h / 2 - 1, cy + h / 2 + 1); pen_off(&p); break;
    case G_MIN: box(dc, cx - h / 2, cy, cx + h / 2 + 1, cy + lw, c); break;
    case G_MAX:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h / 2, cy - h / 2, cx + h / 2 + 1, cy + h / 2 + 1); pen_off(&p); break;
    case G_RESTORE:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h / 2, cy - h / 6, cx + h / 6 + 1, cy + h / 2 + 1);
        MoveToEx(dc, cx - h / 6, cy - h / 6, NULL); LineTo(dc, cx - h / 6, cy - h / 2); LineTo(dc, cx + h / 2 + 1, cy - h / 2); LineTo(dc, cx + h / 2 + 1, cy + h / 6 + 1); LineTo(dc, cx + h / 6 + 1, cy + h / 6 + 1); pen_off(&p); break;
    case G_PIN:
        pen_on(&p, dc, c, lw, 0); Ellipse(dc, cx - h / 3, cy - h / 2, cx + h / 3, cy + h / 8);
        MoveToEx(dc, cx, cy + h / 8, NULL); LineTo(dc, cx, cy + h * 6 / 10); pen_off(&p); break;
    case G_SNAP:
        pen_on(&p, dc, c, lw, 0); Rectangle(dc, cx - h / 2, cy - h / 2, cx + h / 2 + 1, cy + h / 2 + 1);
        MoveToEx(dc, cx, cy - h / 2, NULL); LineTo(dc, cx, cy + h / 2 + 1); pen_off(&p); break;
    case G_SHADE:
        pen_on(&p, dc, c, lw, 0); MoveToEx(dc, cx - h / 2, cy - h / 4, NULL); LineTo(dc, cx, cy + h / 4); LineTo(dc, cx + h / 2, cy - h / 4); pen_off(&p); break;
    case G_PLAY_: { pen_on(&p, dc, c, lw, 1); POINT tri[3] = { { cx - h / 3, cy - h * 6 / 10 }, { cx - h / 3, cy + h * 6 / 10 }, { cx + h * 6 / 10, cy } }; Polygon(dc, tri, 3); pen_off(&p); break; }
    default: break;
    }
    (void)R4;
}
