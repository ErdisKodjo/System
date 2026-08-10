#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "../include/loader.h"

/**
 * @file app_detector.c
 * @brief Detect the executable type of a file by inspecting magic bytes.
 *
 * Magic bytes recognized:
 *   - PE:       "MZ"            (0x4D5A)         -> APP_TYPE_WINDOWS
 *   - ELF:      "\x7FELF"                        -> APP_TYPE_LINUX
 *   - Mach-O:   0xFEEDFACE / 0xFEEDFACF /
 *               0xBEBAFECA / 0xCFFAEDFE / 0xCEFAEDFE  -> APP_TYPE_MACOS
 *   - DEX:      "dex\n035\0"                     -> APP_TYPE_ANDROID
 *   - HarmonyOS: ZIP "PK\x03\x04" + .hap/.hsp ext -> APP_TYPE_HARMONY
 *
 * If nothing matches, APP_TYPE_UNKNOWN is returned. The detection works
 * either on a file path (AppDetect) or an in-memory buffer
 * (AppDetectBuffer).
 */

/* ------------------------------------------------------------------ */
/* Magic constants                                                    */
/* ------------------------------------------------------------------ */

static const uint8_t  PE_MAGIC[2]      = { 0x4D, 0x5A };
static const uint8_t  ELF_MAGIC[4]     = { 0x7F, 'E', 'L', 'F' };
static const uint8_t  DEX_MAGIC[8]     = { 'd','e','x','\n','0','3','5','\0' };
static const uint8_t  ZIP_MAGIC[4]     = { 0x50, 0x4B, 0x03, 0x04 };

#define MACHO_BE_32   0xFEEDFACEu   /* big-endian 32-bit Mach-O   */
#define MACHO_BE_64   0xFEEDFACFu   /* big-endian 64-bit Mach-O   */
#define MACHO_LE_32   0xCEFAEDFEu   /* little-endian 32-bit       */
#define MACHO_LE_64   0xCFFAEDFEu   /* little-endian 64-bit       */
#define MACHO_FAT_BE  0xCAFEBABEu   /* big-endian fat binary      */
#define MACHO_FAT_LE  0xBEBAFECAu   /* little-endian fat binary   */

/* ------------------------------------------------------------------ */
/* Internal: detect from a buffer                                     */
/* ------------------------------------------------------------------ */

static app_type_t detect_buffer(const uint8_t *buf, size_t len)
{
    if (!buf || len < 4)
        return APP_TYPE_UNKNOWN;

    /* ELF check (4 bytes) */
    if (memcmp(buf, ELF_MAGIC, sizeof(ELF_MAGIC)) == 0)
        return APP_TYPE_LINUX;

    /* PE check (2 bytes) */
    if (memcmp(buf, PE_MAGIC, sizeof(PE_MAGIC)) == 0)
        return APP_TYPE_WINDOWS;

    /* Mach-O check (4 bytes, big-endian or little-endian magic) */
    if (len >= 4) {
        uint32_t magic = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                         ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
        if (magic == MACHO_BE_32 || magic == MACHO_BE_64 ||
            magic == MACHO_FAT_BE)
            return APP_TYPE_MACOS;
        uint32_t le_magic =  (uint32_t)buf[0]        |
                            ((uint32_t)buf[1] << 8)  |
                            ((uint32_t)buf[2] << 16) |
                            ((uint32_t)buf[3] << 24);
        if (le_magic == MACHO_LE_32 || le_magic == MACHO_LE_64 ||
            le_magic == MACHO_FAT_LE)
            return APP_TYPE_MACOS;
    }

    /* DEX check (8 bytes) */
    if (len >= 8 && memcmp(buf, DEX_MAGIC, sizeof(DEX_MAGIC)) == 0)
        return APP_TYPE_ANDROID;

    /* HarmonyOS HAP/HSP: ZIP + .hap/.hsp extension is checked at the
     * path level; here we return HARMONY only if buffer looks like ZIP.
     * Final classification (vs Android APK, also ZIP) is done by AppDetect
     * based on file extension. */
    if (len >= 4 && memcmp(buf, ZIP_MAGIC, sizeof(ZIP_MAGIC)) == 0)
        return APP_TYPE_HARMONY;

    return APP_TYPE_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Internal: detect from a path (magic + extension)                   */
/* ------------------------------------------------------------------ */

static int has_suffix(const char *path, const char *suffix)
{
    size_t pl = strlen(path);
    size_t sl = strlen(suffix);
    if (pl < sl) return 0;
    return strcmp(path + pl - sl, suffix) == 0;
}

app_type_t AppDetectBuffer(const void *buf, size_t len)
{
    return detect_buffer((const uint8_t *)buf, len);
}

app_type_t AppDetect(const char *path)
{
    FILE *fp;
    uint8_t hdr[64];
    size_t n;
    app_type_t by_magic;

    if (!path)
        return APP_TYPE_UNKNOWN;

    /* Extension-based hints for HarmonyOS packages (HAP/HSP) since
     * they are ZIP files and could be confused with Android APKs. */
    if (has_suffix(path, ".hap") || has_suffix(path, ".hsp"))
        return APP_TYPE_HARMONY;

    fp = fopen(path, "rb");
    if (!fp)
        return APP_TYPE_UNKNOWN;

    n = fread(hdr, 1, sizeof(hdr), fp);
    fclose(fp);

    by_magic = detect_buffer(hdr, n);
    if (by_magic != APP_TYPE_UNKNOWN)
        return by_magic;

    /* If the magic check returned HARMONY (ZIP) but the extension was
     * .apk, reclassify as Android. */
    if (by_magic == APP_TYPE_HARMONY && has_suffix(path, ".apk"))
        return APP_TYPE_ANDROID;

    /* Script shebang -> treat as Linux (shell/python/etc.) */
    if (n >= 2 && hdr[0] == '#' && hdr[1] == '!')
        return APP_TYPE_LINUX;

    /* Android APK: ZIP file with .apk extension */
    if (has_suffix(path, ".apk") && n >= 4 &&
        memcmp(hdr, ZIP_MAGIC, sizeof(ZIP_MAGIC)) == 0)
        return APP_TYPE_ANDROID;

    return APP_TYPE_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* Op table                                                           */
/* ------------------------------------------------------------------ */

static const loader_ops_t g_loader_ops = {
    .detect        = AppDetect,
    .analyze       = FormatAnalyze,
    .resolve_deps  = ResolveDeps,
    .load          = IntelligentLoad,
};

const loader_ops_t *LoaderGetOps(void)
{
    return &g_loader_ops;
}
