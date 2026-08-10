/**
 * @file CGImage.c
 * @brief Quartz 2D bitmap image.
 *
 * Stores a copy of the pixel buffer in 32-bit RGBA format. The
 * backend rasteriser reads the buffer via CGImageGetData().
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Image structure                                                     */
/* ------------------------------------------------------------------ */

struct cg_image_s {
    int       width;
    int       height;
    int       bits_per_component;
    int       bits_per_pixel;
    int       bytes_per_row;
    uint8_t  *data;
    int       refcount;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static size_t image_data_size(int w, int h, int bpp) {
    int bytes_per_pixel = bpp / 8;
    return (size_t)w * (size_t)h * (size_t)bytes_per_pixel;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

CGImage *CGImageCreate(int width, int height, int bits_per_component,
                       int bits_per_pixel, const uint8_t *data) {
    if (width <= 0 || height <= 0 || bits_per_pixel <= 0) return NULL;
    CGImage *img = (CGImage *)calloc(1, sizeof *img);
    if (!img) return NULL;
    img->width              = width;
    img->height             = height;
    img->bits_per_component = bits_per_component;
    img->bits_per_pixel     = bits_per_pixel;
    img->bytes_per_row      = (bits_per_pixel / 8) * width;
    img->refcount           = 1;
    size_t n = image_data_size(width, height, bits_per_pixel);
    img->data = (uint8_t *)malloc(n ? n : 1);
    if (!img->data) {
        free(img);
        return NULL;
    }
    if (data) {
        memcpy(img->data, data, n);
    } else {
        memset(img->data, 0, n);
    }
    return img;
}

CGImage *CGImageRetain(CGImage *img) {
    if (img) __sync_fetch_and_add(&img->refcount, 1);
    return img;
}

void CGImageRelease(CGImage *img) {
    if (!img) return;
    if (__sync_fetch_and_sub(&img->refcount, 1) == 1) {
        free(img->data);
        free(img);
    }
}

size_t CGImageGetWidth(CGImage *img) {
    return img ? (size_t)img->width : 0;
}

size_t CGImageGetHeight(CGImage *img) {
    return img ? (size_t)img->height : 0;
}

int CGImageGetBitsPerPixel(CGImage *img) {
    return img ? img->bits_per_pixel : 0;
}

int CGImageGetBitsPerComponent(CGImage *img) {
    return img ? img->bits_per_component : 0;
}

int CGImageGetBytesPerRow(CGImage *img) {
    return img ? img->bytes_per_row : 0;
}

const uint8_t *CGImageGetData(CGImage *img) {
    return img ? img->data : NULL;
}

CGImage *CGImageCreateSubimage(CGImage *src, CGRect region) {
    if (!src) return NULL;
    int x = (int)region.origin.x;
    int y = (int)region.origin.y;
    int w = (int)region.size.width;
    int h = (int)region.size.height;
    if (x < 0 || y < 0 || w <= 0 || h <= 0 ||
        x + w > src->width || y + h > src->height) {
        return NULL;
    }
    int bpp = src->bits_per_pixel;
    int bytes_per_pixel = bpp / 8;
    CGImage *sub = CGImageCreate(w, h, src->bits_per_component, bpp, NULL);
    if (!sub) return NULL;
    for (int row = 0; row < h; row++) {
        const uint8_t *src_row =
            src->data + (size_t)(y + row) * src->bytes_per_row
                       + (size_t)x * bytes_per_pixel;
        uint8_t *dst_row =
            sub->data + (size_t)row * sub->bytes_per_row;
        memcpy(dst_row, src_row, (size_t)w * bytes_per_pixel);
    }
    return sub;
}

bool CGImageIsOpaque(CGImage *img) {
    if (!img || !img->data) return true;
    if (img->bits_per_pixel != 32) return true;
    /* Scan all alpha bytes.                                           */
    for (int y = 0; y < img->height; y++) {
        const uint8_t *row = img->data + (size_t)y * img->bytes_per_row;
        for (int x = 0; x < img->width; x++) {
            if (row[x * 4 + 3] != 255) return false;
        }
    }
    return true;
}
