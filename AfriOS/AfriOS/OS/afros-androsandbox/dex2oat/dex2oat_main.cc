/*
 * dex2oat/dex2oat_main.cc — AOT compiler entry point.
 *
 * `dex2oat` is the Android ahead-of-time compiler: it reads one or more
 * .dex files, runs them through the optimizing compiler, and writes an
 * .oat file (an ELF shared object containing the compiled code and a
 * copy of the DEX) plus optionally an .art image (a prebuilt heap).
 *
 * This module mirrors the upstream command-line interface (-dex-file,
 * -oat-file, -instruction-set, -compiler-filter) and drives the
 * CompilerDriver (compiler_driver.cc) to perform the actual work. In
 * the sandbox the output file is a stub ELF header so the loader can
 * recognise it, but the full compile pipeline runs.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
    status_t CompilerDriverCompile(const char *dex_path,
                                   const char *out_path,
                                   const char *isa,
                                   const char *filter);
    status_t CompilerDriverSetFilter(const char *filter);
}

struct Dex2OatOptions {
    std::string dex_file;
    std::string oat_file;
    std::string instruction_set;  /* arm64, x86_64, ... */
    std::string compiler_filter;  /* speed, speed-profile, quicken, verify, ... */
    std::string image_file;
    std::vector<std::string> runtime_args;
    bool        help;
};

static void print_help(const char *prog) {
    std::printf(
        "Usage: %s [options]\n"
        "Options:\n"
        "  --dex-file=<file>         input .dex/.jar/.apk\n"
        "  --oat-file=<file>         output .oat (ELF)\n"
        "  --instruction-set=<isa>   target ISA (arm64|x86_64|riscv64)\n"
        "  --compiler-filter=<f>     speed|speed-profile|quicken|verify|assume-verified\n"
        "  --image=<file>            prebuilt image to use\n"
        "  --runtime-arg <arg>       extra argument to forward to the runtime\n"
        "  -h, --help                print this help\n",
        prog);
}

static bool parse_opts(int argc, char **argv, Dex2OatOptions &out) {
    out.instruction_set = "arm64";
    out.compiler_filter = "speed";
    out.help = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!std::strcmp(a, "-h") || !std::strcmp(a, "--help")) {
            out.help = true; return true;
        } else if (!std::strncmp(a, "--dex-file=", 11)) {
            out.dex_file = a + 11;
        } else if (!std::strncmp(a, "--oat-file=", 11)) {
            out.oat_file = a + 11;
        } else if (!std::strncmp(a, "--instruction-set=", 18)) {
            out.instruction_set = a + 18;
        } else if (!std::strncmp(a, "--compiler-filter=", 18)) {
            out.compiler_filter = a + 18;
        } else if (!std::strncmp(a, "--image=", 8)) {
            out.image_file = a + 8;
        } else if (!std::strcmp(a, "--runtime-arg") && i + 1 < argc) {
            out.runtime_args.push_back(argv[++i]);
        } else if (a[0] == '-') {
            /* Unknown option: ignored (upstream is permissive here). */
        }
    }
    return !out.dex_file.empty() && !out.oat_file.empty();
}

extern "C" status_t Dex2OatMain(int argc, char **argv) {
    Dex2OatOptions opt;
    if (!parse_opts(argc, argv, opt)) {
        if (opt.help) { print_help(argv[0]); return OK; }
        std::fprintf(stderr, "dex2oat: --dex-file and --oat-file are required\n");
        print_help(argv[0]);
        return BAD_VALUE;
    }
    status_t s = CompilerDriverSetFilter(opt.compiler_filter.c_str());
    if (s != OK) {
        std::fprintf(stderr, "dex2oat: unknown filter '%s'\n",
                     opt.compiler_filter.c_str());
        return s;
    }
    s = CompilerDriverCompile(opt.dex_file.c_str(),
                              opt.oat_file.c_str(),
                              opt.instruction_set.c_str(),
                              opt.compiler_filter.c_str());
    if (s != OK) {
        std::fprintf(stderr, "dex2oat: compile failed: %d\n", s);
        return s;
    }
    std::printf("dex2oat: wrote %s (%s, %s)\n",
                opt.oat_file.c_str(),
                opt.instruction_set.c_str(),
                opt.compiler_filter.c_str());
    return OK;
}

/* C-callable entry for the AfriOS package installer. */
extern "C" int Dex2OatEntry(int argc, char **argv) {
    return (int)Dex2OatMain(argc, argv);
}
