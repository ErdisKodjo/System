/**
 * @file CGPath.c
 * @brief Quartz 2D path object.
 *
 * Stores a list of move/line/close operations that can be replayed
 * into a CGContext or rasterised by a backend.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Path element                                                        */
/* ------------------------------------------------------------------ */

typedef struct path_element_s {
    CGPathElementType   type;
    CGPoint             points[4]; /* up to 4 for rect corners       */
    int                 n_points;
    struct path_element_s *next;
} path_element_t;

struct cg_path_s {
    path_element_t *head;
    path_element_t *tail;
    int             count;
    int             refcount;
    CGPoint         current;
    bool            has_current;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static path_element_t *element_new(CGPathElementType t) {
    path_element_t *e = (path_element_t *)calloc(1, sizeof *e);
    if (!e) return NULL;
    e->type = t;
    return e;
}

static void path_append(CGPath *p, path_element_t *e) {
    if (!p->head) {
        p->head = p->tail = e;
    } else {
        p->tail->next = e;
        p->tail = e;
    }
    p->count++;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

CGRect CGRectMake(float x, float y, float w, float h) {
    CGRect r;
    r.origin.x    = x;
    r.origin.y    = y;
    r.size.width  = w;
    r.size.height = h;
    return r;
}

CGPoint CGPointMake(float x, float y) {
    CGPoint p; p.x = x; p.y = y; return p;
}

CGSize CGSizeMake(float w, float h) {
    CGSize s; s.width = w; s.height = h; return s;
}

bool CGRectIsEmpty(CGRect r) {
    return r.size.width <= 0.0f || r.size.height <= 0.0f;
}

bool CGRectEqualToRect(CGRect a, CGRect b) {
    return a.origin.x == b.origin.x && a.origin.y == b.origin.y &&
           a.size.width == b.size.width && a.size.height == b.size.height;
}

bool CGRectContainsPoint(CGRect r, CGPoint p) {
    return p.x >= r.origin.x && p.x <= r.origin.x + r.size.width &&
           p.y >= r.origin.y && p.y <= r.origin.y + r.size.height;
}

CGRect CGRectUnion(CGRect a, CGRect b) {
    if (CGRectIsEmpty(a)) return b;
    if (CGRectIsEmpty(b)) return a;
    float xmin = a.origin.x < b.origin.x ? a.origin.x : b.origin.x;
    float ymin = a.origin.y < b.origin.y ? a.origin.y : b.origin.y;
    float xmax = a.origin.x + a.size.width;
    float bmax = b.origin.x + b.size.width;
    float x2   = xmax > bmax ? xmax : bmax;
    float ymax = a.origin.y + a.size.height;
    float y2b  = b.origin.y + b.size.height;
    float y2m  = ymax > y2b ? ymax : y2b;
    return CGRectMake(xmin, ymin, x2 - xmin, y2m - ymin);
}

CGPath *CGPathCreate(void) {
    CGPath *p = (CGPath *)calloc(1, sizeof *p);
    if (!p) return NULL;
    p->refcount = 1;
    return p;
}

CGPath *CGPathRetain(CGPath *p) {
    if (p) __sync_fetch_and_add(&p->refcount, 1);
    return p;
}

void CGPathRelease(CGPath *p) {
    if (!p) return;
    if (__sync_fetch_and_sub(&p->refcount, 1) == 1) {
        path_element_t *e = p->head;
        while (e) {
            path_element_t *next = e->next;
            free(e);
            e = next;
        }
        free(p);
    }
}

void CGPathMoveToPoint(CGPath *p, CGPoint pt) {
    if (!p) return;
    path_element_t *e = element_new(kCGPathElementMoveToPoint);
    if (!e) return;
    e->points[0] = pt;
    e->n_points   = 1;
    path_append(p, e);
    p->current     = pt;
    p->has_current = true;
}

void CGPathAddLineToPoint(CGPath *p, CGPoint pt) {
    if (!p) return;
    if (!p->has_current) { CGPathMoveToPoint(p, pt); return; }
    path_element_t *e = element_new(kCGPathElementAddLineToPoint);
    if (!e) return;
    e->points[0] = pt;
    e->n_points   = 1;
    path_append(p, e);
    p->current = pt;
}

void CGPathAddRect(CGPath *p, CGRect r) {
    if (!p) return;
    path_element_t *e = element_new(kCGPathElementAddRect);
    if (!e) return;
    e->points[0] = r.origin;
    e->points[1] = CGPointMake(r.origin.x + r.size.width, r.origin.y);
    e->points[2] = CGPointMake(r.origin.x + r.size.width,
                               r.origin.y + r.size.height);
    e->points[3] = CGPointMake(r.origin.x, r.origin.y + r.size.height);
    e->n_points   = 4;
    path_append(p, e);
    p->current     = e->points[3];
    p->has_current = true;
}

void CGPathCloseSubpath(CGPath *p) {
    if (!p) return;
    path_element_t *e = element_new(kCGPathElementCloseSubpath);
    if (!e) return;
    path_append(p, e);
    p->has_current = false;
}

bool CGPathIsEmpty(CGPath *p) {
    return !p || p->count == 0;
}

int CGPathGetCount(CGPath *p) {
    return p ? p->count : 0;
}

void CGPathEnumerate(CGPath *p,
                     void (*cb)(CGPathElementType t,
                                const CGPoint *pts, int n, void *ctx),
                     void *ctx) {
    if (!p || !cb) return;
    for (path_element_t *e = p->head; e; e = e->next) {
        cb(e->type, e->points, e->n_points, ctx);
    }
}
