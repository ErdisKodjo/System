#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "../include/loader.h"

/**
 * @file format_analyzer.c
 * @brief Deeper inspection of a binary after AppDetect has identified it.
 *
 * For each supported type we extract:
 *   - PE:       subsystem (GUI / CUI), machine type (arch)
 *   - ELF:      interpreter (PT_INTERP / ld-linux), class (32/64-bit), machine
 *   - Mach-O:   cputype (arm64 / x86_64), magic class
 *   - DEX:      version string (035, 036, 037, 038, 039)
 *   - HarmonyOS:bundle id / entry ability parsed from module.json
 */

/* ------------------------------------------------------------------ */
/* PE structures (subset)                                             */
/* ------------------------------------------------------------------ */

#pragma pack(push, 2)
typedef struct {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
} pe_dos_hdr_t;

typedef struct {
    uint16_t machine;
    uint16_t number_of_sections;
    uint32_t time_date_stamp;
    uint32_t pointer_to_symbol_table;
    uint32_t number_of_symbols;
    uint16_t size_of_optional_header;
    uint16_t characteristics;
} pe_file_hdr_t;

typedef struct {
    uint16_t magic;
    uint8_t  major_linker;
    uint8_t  minor_linker;
    uint32_t size_of_code;
    uint32_t size_of_init_data;
    uint32_t size_of_uninit_data;
    uint32_t address_of_entry;
    uint32_t base_of_code;
    /* PE32 only: base_of_data here */
    /* we read subsystem via fixed offset below */
} pe_opt_hdr_t;

#define PE_MACHINE_UNKNOWN 0x0000
#define PE_MACHINE_I386    0x014C
#define PE_MACHINE_AMD64   0x8664
#define PE_MACHINE_ARM     0x01C0
#define PE_MACHINE_ARM64   0xAA64
#define PE_MACHINE_RISCV64 0x5064

#pragma pack(pop)

/* ------------------------------------------------------------------ */
/* ELF structures (subset)                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    /* ... */
} elf_hdr_t;

#define ELFOSABI_NONE    0
#define ELFOSABI_LINUX   3
#define PT_INTERP        3

#define EM_386     3
#define EM_ARM    40
#define EM_X86_64 62
#define EM_AARCH64 183
#define EM_RISCV  243

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static int read_file_head(const char *path, void *buf, size_t len)
{
    FILE *fp = fopen(path, "rb");
    size_t n;
    if (!fp) return -1;
    n = fread(buf, 1, len, fp);
    fclose(fp);
    return (int)n;
}

static void safe_copy(char *dst, size_t cap, const char *src)
{
    size_t l = src ? strlen(src) : 0;
    if (l >= cap) l = cap - 1;
    memcpy(dst, src, l);
    dst[l] = '\0';
}

/* ------------------------------------------------------------------ */
/* PE analysis                                                        */
/* ------------------------------------------------------------------ */

static format_info_t analyze_pe(const uint8_t *buf, size_t len)
{
    format_info_t fi;
    const pe_dos_hdr_t *dos;
    const pe_file_hdr_t *fh;
    const uint8_t *opt;
    uint32_t opt_off;
    uint16_t opt_magic;

    memset(&fi, 0, sizeof(fi));
    fi.type = APP_TYPE_WINDOWS;
    if (len < sizeof(pe_dos_hdr_t) + 4)
        return fi;
    dos = (const pe_dos_hdr_t *)buf;
    if (dos->e_lfanew + 4 + sizeof(pe_file_hdr_t) > len)
        return fi;
    /* PE\0\0 = "PE\0\0" */
    if (memcmp(buf + dos->e_lfanew, "PE\0\0", 4) != 0)
        return fi;

    fh = (const pe_file_hdr_t *)(buf + dos->e_lfanew + 4);
    switch (fh->machine) {
    case PE_MACHINE_I386:    fi.arch = APP_ARCH_X86;     fi.bits = 32; break;
    case PE_MACHINE_AMD64:   fi.arch = APP_ARCH_X86_64;  fi.bits = 64; break;
    case PE_MACHINE_ARM:     fi.arch = APP_ARCH_ARM32;   fi.bits = 32; break;
    case PE_MACHINE_ARM64:   fi.arch = APP_ARCH_ARM64;   fi.bits = 64; break;
    case PE_MACHINE_RISCV64: fi.arch = APP_ARCH_RISCV64; fi.bits = 64; break;
    default:                 fi.arch = APP_ARCH_UNKNOWN; fi.bits = 0;  break;
    }

    opt_off = dos->e_lfanew + 4 + sizeof(pe_file_hdr_t);
    opt = buf + opt_off;
    if (opt_off + 2 > len)
        return fi;
    opt_magic = opt[0] | (opt[1] << 8);
    fi.format_version = opt_magic; /* 0x10b = PE32, 0x20b = PE32+ */

    /* Subsystem is at offset 68 (PE32) or 68 (PE32+, same offset because
     * the optional header up to subsystem is identical). */
    if (opt_off + 70 <= len)
        fi.subsystem = opt[68] | (opt[69] << 8);

    return fi;
}

/* ------------------------------------------------------------------ */
/* ELF analysis                                                       */
/* ------------------------------------------------------------------ */

static format_info_t analyze_elf(const uint8_t *buf, size_t len)
{
    format_info_t fi;
    const elf_hdr_t *eh;
    uint8_t ei_class;
    uint16_t machine;
    const uint8_t *p;
    const uint8_t *end;
    uint32_t phoff;
    uint16_t phentsize, phnum;

    memset(&fi, 0, sizeof(fi));
    fi.type = APP_TYPE_LINUX;
    if (len < sizeof(elf_hdr_t))
        return fi;
    eh = (const elf_hdr_t *)buf;
    ei_class = eh->e_ident[4];        /* 1 = ELF32, 2 = ELF64 */
    machine = eh->e_machine;
    fi.bits = (ei_class == 2) ? 64 : 32;
    switch (machine) {
    case EM_386:      fi.arch = APP_ARCH_X86;     break;
    case EM_ARM:      fi.arch = APP_ARCH_ARM32;   break;
    case EM_X86_64:   fi.arch = APP_ARCH_X86_64;  break;
    case EM_AARCH64:  fi.arch = APP_ARCH_ARM64;   break;
    case EM_RISCV:    fi.arch = APP_ARCH_RISCV64; break;
    default:          fi.arch = APP_ARCH_UNKNOWN; break;
    }

    if (ei_class == 2) {
        if (len < 64) return fi;
        phoff     = *(const uint32_t *)(buf + 32);
        phentsize = *(const uint16_t *)(buf + 54);
        phnum     = *(const uint16_t *)(buf + 56);
    } else {
        if (len < 52) return fi;
        phoff     = *(const uint32_t *)(buf + 28);
        phentsize = *(const uint16_t *)(buf + 42);
        phnum     = *(const uint16_t *)(buf + 44);
    }

    /* Walk program headers looking for PT_INTERP. */
    if (phoff == 0 || phentsize == 0 || phnum == 0)
        return fi;
    end = buf + len;
    for (p = buf + phoff; phnum > 0 && p + 8 <= end; p += phentsize, phnum--) {
        uint32_t p_type = *(const uint32_t *)(p + 0);
        uint32_t p_offset;
        uint32_t p_filesz;
        if (ei_class == 2) {
            p_offset = *(const uint32_t *)(p + 8);
            p_filesz = *(const uint32_t *)(p + 32);
        } else {
            p_offset = *(const uint32_t *)(p + 4);
            p_filesz = *(const uint32_t *)(p + 16);
        }
        if (p_type != PT_INTERP)
            continue;
        if (p_offset + p_filesz > len || p_filesz == 0)
            continue;
        {
            size_t l = p_filesz;
            if (l >= MAX_INTERP_LEN) l = MAX_INTERP_LEN - 1;
            memcpy(fi.interpreter, buf + p_offset, l);
            fi.interpreter[l] = '\0';
            if (l > 0 && fi.interpreter[l - 1] == '\0')
                fi.interpreter[l - 1] = '\0';
        }
        break;
    }
    return fi;
}

/* ------------------------------------------------------------------ */
/* Mach-O analysis                                                    */
/* ------------------------------------------------------------------ */

static format_info_t analyze_macho(const uint8_t *buf, size_t len)
{
    format_info_t fi;
    uint32_t magic;

    memset(&fi, 0, sizeof(fi));
    fi.type = APP_TYPE_MACOS;
    if (len < 8)
        return fi;
    magic = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
            ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
    if (magic == 0xFEEDFACE || magic == 0xFEEDFACF) {
        /* big-endian header */
        fi.bits = (magic == 0xFEEDFACF) ? 64 : 32;
        if (len >= 8) {
            uint32_t cputype = ((uint32_t)buf[4] << 24) |
                               ((uint32_t)buf[5] << 16) |
                               ((uint32_t)buf[6] << 8)  |
                                (uint32_t)buf[7];
            if (cputype == 0x01000007)      fi.arch = APP_ARCH_X86_64;
            else if (cputype == 0x0100000C) fi.arch = APP_ARCH_ARM64;
            else if (cputype == 0x00000007) fi.arch = APP_ARCH_X86;
            else if (cputype == 0x0000000C) fi.arch = APP_ARCH_ARM32;
        }
    } else {
        /* little-endian header */
        uint32_t le_magic =  (uint32_t)buf[0]        |
                            ((uint32_t)buf[1] << 8)  |
                            ((uint32_t)buf[2] << 16) |
                            ((uint32_t)buf[3] << 24);
        fi.bits = (le_magic == 0xCFFAEDFE) ? 64 : 32;
        if (len >= 8) {
            uint32_t cputype =  (uint32_t)buf[4]        |
                               ((uint32_t)buf[5] << 8)  |
                               ((uint32_t)buf[6] << 16) |
                               ((uint32_t)buf[7] << 24);
            if (cputype == 0x01000007)      fi.arch = APP_ARCH_X86_64;
            else if (cputype == 0x0100000C) fi.arch = APP_ARCH_ARM64;
            else if (cputype == 0x00000007) fi.arch = APP_ARCH_X86;
            else if (cputype == 0x0000000C) fi.arch = APP_ARCH_ARM32;
        }
    }
    fi.format_version = 0; /* Mach-O has no version field */
    return fi;
}

/* ------------------------------------------------------------------ */
/* DEX analysis                                                       */
/* ------------------------------------------------------------------ */

static format_info_t analyze_dex(const uint8_t *buf, size_t len)
{
    format_info_t fi;

    memset(&fi, 0, sizeof(fi));
    fi.type = APP_TYPE_ANDROID;
    fi.arch = APP_ARCH_ARM64; /* ART runs on device arch; assume host arch */
    fi.bits = 32;             /* DEX bytecode is 32-bit register based */
    if (len >= 8) {
        char v[4] = { (char)buf[4], (char)buf[5], (char)buf[6], 0 };
        fi.format_version = (uint32_t)((v[0] - '0') * 100 +
                                       (v[1] - '0') * 10  +
                                       (v[2] - '0'));
    }
    return fi;
}

/* ------------------------------------------------------------------ */
/* HarmonyOS analysis (ZIP + module.json)                             */
/* ------------------------------------------------------------------ */

/* Very small JSON field extractor: finds "key":"value" pattern. */
static int json_get_string(const char *json, size_t len,
                           const char *key, char *out, size_t cap)
{
    char pattern[64];
    size_t i, klen;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    klen = strlen(pattern);
    for (i = 0; i + klen + 2 < len; i++) {
        if (memcmp(json + i, pattern, klen) == 0) {
            size_t j = i + klen;
            while (j < len && (json[j] == ' ' || json[j] == ':' ||
                               json[j] == '\t' || json[j] == '\n'))
                j++;
            if (j < len && json[j] == '"') {
                size_t n = 0;
                j++;
                while (j < len && json[j] != '"' && n + 1 < cap)
                    out[n++] = json[j++];
                out[n] = '\0';
                return 0;
            }
        }
    }
    return -1;
}

static format_info_t analyze_harmony(const char *path)
{
    format_info_t fi;
    char cmd[1024];
    FILE *pipe;
    char json[8192];
    size_t n;

    memset(&fi, 0, sizeof(fi));
    fi.type = APP_TYPE_HARMONY;
    fi.arch = APP_ARCH_ARM64; /* HarmonyOS targets are typically ARM64 */
    fi.bits = 64;

    /* Best-effort: use unzip to extract module.json to stdout. In a real
     * deploy this would use libminizip; here we shell out for portability. */
    snprintf(cmd, sizeof(cmd),
             "unzip -p \"%s\" module.json 2>/dev/null", path);
    pipe = popen(cmd, "r");
    if (!pipe)
        return fi;
    n = fread(json, 1, sizeof(json) - 1, pipe);
    pclose(pipe);
    if (n == 0)
        return fi;
    json[n] = '\0';

    json_get_string(json, n, "bundleName", fi.bundle_id, sizeof(fi.bundle_id));
    json_get_string(json, n, "name",        fi.entry_name, sizeof(fi.entry_name));
    return fi;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

format_info_t FormatAnalyze(const char *path, app_type_t type)
{
    format_info_t fi;
    uint8_t buf[4096];
    int n;

    memset(&fi, 0, sizeof(fi));
    if (!path)
        return fi;

    if (type == APP_TYPE_HARMONY)
        return analyze_harmony(path);

    n = read_file_head(path, buf, sizeof(buf));
    if (n <= 0)
        return fi;

    switch (type) {
    case APP_TYPE_WINDOWS: return analyze_pe(buf, (size_t)n);
    case APP_TYPE_LINUX:   return analyze_elf(buf, (size_t)n);
    case APP_TYPE_MACOS:   return analyze_macho(buf, (size_t)n);
    case APP_TYPE_ANDROID: return analyze_dex(buf, (size_t)n);
    default:
        /* If unknown, run all analyzers to try to classify. */
        if (n >= 4 && buf[0] == 'M' && buf[1] == 'Z')
            return analyze_pe(buf, (size_t)n);
        if (n >= 4 && buf[0] == 0x7F && buf[1] == 'E')
            return analyze_elf(buf, (size_t)n);
        if (n >= 4 && (buf[0] == 0xFE || buf[0] == 0xCF || buf[0] == 0xCE))
            return analyze_macho(buf, (size_t)n);
        if (n >= 8 && memcmp(buf, "dex\n", 4) == 0)
            return analyze_dex(buf, (size_t)n);
        break;
    }
    return fi;
}
