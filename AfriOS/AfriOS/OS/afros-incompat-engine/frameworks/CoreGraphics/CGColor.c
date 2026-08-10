/**
 * @file CGColor.c
 * @brief Quartz 2D color: RGB, RGBA, Grayscale.
 *
 * Colors are reference-counted. Components are clamped to [0, 1].
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Color structure                                                     */
/* ------------------------------------------------------------------ */

struct cg_color_s {
    float  components[4];   /* r, g, b, a (or w, w, w, a for gray)    */
    int    n_components;    /* 2 for gray+alpha, 4 for rgba           */
    int    refcount;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static float clampf(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static CGColor *color_alloc(int n) {
    CGColor *c = (CGColor *)calloc(1, sizeof *c);
    if (!c) return NULL;
    c->n_components = n;
    c->refcount     = 1;
    c->components[3] = 1.0f;
    return c;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

CGColor *CGColorCreate(float r, float g, float b, float a) {
    CGColor *c = color_alloc(4);
    if (!c) return NULL;
    c->components[0] = clampf(r);
    c->components[1] = clampf(g);
    c->components[2] = clampf(b);
    c->components[3] = clampf(a);
    return c;
}

CGColor *CGColorCreateGray(float w, float a) {
    CGColor *c = color_alloc(2);
    if (!c) return NULL;
    c->components[0] = clampf(w);
    c->components[1] = clampf(w);
    c->components[2] = clampf(w);
    c->components[3] = clampf(a);
    return c;
}

CGColor *CGColorCreateCopy(CGColor *src) {
    if (!src) return NULL;
    CGColor *c = color_alloc(src->n_components);
    if (!c) return NULL;
    memcpy(c->components, src->components, sizeof c->components);
    return c;
}

CGColor *CGColorRetain(CGColor *c) {
    if (c) __sync_fetch_and_add(&c->refcount, 1);
    return c;
}

void CGColorRelease(CGColor *c) {
    if (!c) return;
    if (__sync_fetch_and_sub(&c->refcount, 1) == 1) {
        free(c);
    }
}

void CGColorGetComponents(CGColor *c, float out[4]) {
    if (!c || !out) return;
    out[0] = c->components[0];
    out[1] = c->components[1];
    out[2] = c->components[2];
    out[3] = c->components[3];
}

int CGColorGetNumberOfComponents(CGColor *c) {
    return c ? c->n_components : 0;
}

float CGColorGetAlpha(CGColor *c) {
    return c ? c->components[3] : 0.0f;
}

bool CGColorEqualToColor(CGColor *a, CGColor *b) {
    if (!a || !b) return false;
    if (a->n_components != b->n_components) return false;
    for (int i = 0; i < 4; i++) {
        if (a->components[i] != b->components[i]) return false;
    }
    return true;
}

CGColor *CGColorGetConstantColor(const char *name) {
    if (!name) return NULL;
    if (strcmp(name, "kCGColorWhite") == 0) {
        return CGColorCreateGray(1.0f, 1.0f);
    }
    if (strcmp(name, "kCGColorBlack") == 0) {
        return CGColorCreateGray(0.0f, 1.0f);
    }
    if (strcmp(name, "kCGColorClear") == 0) {
        return CGColorCreateGray(0.0f, 0.0f);
    }
    return NULL;
}
