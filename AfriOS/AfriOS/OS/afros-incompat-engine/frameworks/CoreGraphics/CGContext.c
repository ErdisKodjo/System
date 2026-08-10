/**
 * @file CGContext.c
 * @brief Quartz 2D drawing context.
 *
 * Maintains a current path, stroke/fill colors and a stroke width.
 * Drawing commands are emitted to a backend callback as text records
 * that can be replayed by any rasteriser (e.g. the in-process
 * software renderer or a GPU backend).
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Context                                                             */
/* ------------------------------------------------------------------ */

struct cg_context_s {
    void    *backend;
    void   (*emit)(void *backend, const char *record);
    CGPath  *path;
    CGColor *stroke_color;
    CGColor *fill_color;
    float    line_width;
    int      refcount;
    bool     has_current_point;
    CGPoint  current_point;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void emit_record(CGContext *ctx, const char *rec) {
    if (ctx && ctx->emit) ctx->emit(ctx->backend, rec);
}

static void format_emit(CGContext *ctx, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    emit_record(ctx, buf);
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

CGContext *CGContextCreate(void *backend, void (*emit)(void *, const char *)) {
    CGContext *ctx = (CGContext *)calloc(1, sizeof *ctx);
    if (!ctx) return NULL;
    ctx->backend      = backend;
    ctx->emit         = emit;
    ctx->path         = CGPathCreate();
    ctx->stroke_color = CGColorCreate(0.0f, 0.0f, 0.0f, 1.0f);
    ctx->fill_color   = CGColorCreate(1.0f, 1.0f, 1.0f, 1.0f);
    ctx->line_width   = 1.0f;
    ctx->refcount     = 1;
    return ctx;
}

CGContext *CGContextRetain(CGContext *ctx) {
    if (ctx) __sync_fetch_and_add(&ctx->refcount, 1);
    return ctx;
}

void CGContextRelease(CGContext *ctx) {
    if (!ctx) return;
    if (__sync_fetch_and_sub(&ctx->refcount, 1) == 1) {
        if (ctx->path)         CGPathRelease(ctx->path);
        if (ctx->stroke_color) CGColorRelease(ctx->stroke_color);
        if (ctx->fill_color)   CGColorRelease(ctx->fill_color);
        free(ctx);
    }
}

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

void CGContextSetStrokeColor(CGContext *ctx, CGColor *c) {
    if (!ctx || !c) return;
    if (ctx->stroke_color) CGColorRelease(ctx->stroke_color);
    ctx->stroke_color = CGColorRetain(c);
    float v[4];
    CGColorGetComponents(c, v);
    format_emit(ctx, "stroke %f %f %f %f", v[0], v[1], v[2], v[3]);
}

void CGContextSetFillColor(CGContext *ctx, CGColor *c) {
    if (!ctx || !c) return;
    if (ctx->fill_color) CGColorRelease(ctx->fill_color);
    ctx->fill_color = CGColorRetain(c);
    float v[4];
    CGColorGetComponents(c, v);
    format_emit(ctx, "fill %f %f %f %f", v[0], v[1], v[2], v[3]);
}

void CGContextSetLineWidth(CGContext *ctx, float w) {
    if (!ctx) return;
    ctx->line_width = w;
    format_emit(ctx, "lineWidth %f", w);
}

void CGContextSaveGState(CGContext *ctx) {
    emit_record(ctx, "save");
}

void CGContextRestoreGState(CGContext *ctx) {
    emit_record(ctx, "restore");
}

/* ------------------------------------------------------------------ */
/* Path construction                                                   */
/* ------------------------------------------------------------------ */

void CGContextMoveToPoint(CGContext *ctx, float x, float y) {
    if (!ctx) return;
    CGPathMoveToPoint(ctx->path, CGPointMake(x, y));
    ctx->current_point     = CGPointMake(x, y);
    ctx->has_current_point = true;
    format_emit(ctx, "moveTo %f %f", x, y);
}

void CGContextAddLineToPoint(CGContext *ctx, float x, float y) {
    if (!ctx) return;
    if (!ctx->has_current_point) { CGContextMoveToPoint(ctx, x, y); return; }
    CGPathAddLineToPoint(ctx->path, CGPointMake(x, y));
    ctx->current_point = CGPointMake(x, y);
    format_emit(ctx, "lineTo %f %f", x, y);
}

void CGContextAddRect(CGContext *ctx, CGRect r) {
    if (!ctx) return;
    CGPathAddRect(ctx->path, r);
    format_emit(ctx, "addRect %f %f %f %f",
                r.origin.x, r.origin.y, r.size.width, r.size.height);
}

void CGContextClosePath(CGContext *ctx) {
    if (!ctx) return;
    CGPathCloseSubpath(ctx->path);
    emit_record(ctx, "closePath");
    ctx->has_current_point = false;
}

void CGContextBeginPath(CGContext *ctx) {
    if (!ctx || !ctx->path) return;
    CGPathRelease(ctx->path);
    ctx->path = CGPathCreate();
    ctx->has_current_point = false;
    emit_record(ctx, "beginPath");
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

void CGContextStrokePath(CGContext *ctx) {
    if (!ctx) return;
    emit_record(ctx, "strokePath");
    CGContextBeginPath(ctx);
}

void CGContextFillPath(CGContext *ctx) {
    if (!ctx) return;
    emit_record(ctx, "fillPath");
    CGContextBeginPath(ctx);
}

void CGContextStrokeRect(CGContext *ctx, CGRect r) {
    if (!ctx) return;
    format_emit(ctx, "strokeRect %f %f %f %f",
                r.origin.x, r.origin.y, r.size.width, r.size.height);
}

void CGContextFillRect(CGContext *ctx, CGRect r) {
    if (!ctx) return;
    format_emit(ctx, "fillRect %f %f %f %f",
                r.origin.x, r.origin.y, r.size.width, r.size.height);
}

void CGContextDrawImage(CGContext *ctx, CGRect r, CGImage *img) {
    if (!ctx || !img) return;
    format_emit(ctx, "drawImage %f %f %f %f %p %d",
                r.origin.x, r.origin.y, r.size.width, r.size.height,
                (void *)img, (int)CGImageGetWidth(img));
}

void CGContextClearRect(CGContext *ctx, CGRect r) {
    if (!ctx) return;
    format_emit(ctx, "clearRect %f %f %f %f",
                r.origin.x, r.origin.y, r.size.width, r.size.height);
}

void CGContextFlush(CGContext *ctx) {
    if (!ctx) return;
    emit_record(ctx, "flush");
}

void CGContextSynchronize(CGContext *ctx) {
    if (!ctx) return;
    emit_record(ctx, "sync");
}
