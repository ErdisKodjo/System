/*
 * dalvikvm/dalvikvm.cc — Command-line entry for the Dalvik/ART VM launcher.
 *
 * `dalvikvm` is the Android command-line tool that boots an ART runtime,
 * loads a .dex file from the classpath, finds a `main(String[])` method,
 * and invokes it. This file mirrors that flow: parse argv, create the
 * ArtRuntime singleton, load the specified .dex via the ClassLinker, look
 * up `main`, run the interpreter loop, and shut down cleanly.
 *
 * In the sandbox we don't actually execute bytecode — DalvikvmMain() goes
 * through all the motions (parsing, loading, lookup) and then returns the
 * ART shutdown status, so callers can verify the launch path end-to-end.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <cstdlib>

/* Forward declarations — defined in art/runtime/ .cc files. */
namespace afros_art {
class ArtRuntime;
}

extern "C" {
    status_t ArtRuntimeStart(int argc, char **argv);
    status_t ArtRuntimeShutdown(void);
    void    *ArtRuntimeGetInstance(void);
    status_t ClassLinkerLookupClass(const char *name, void **out);
    status_t ClassLinkerDefineClass(const char *name, const void *dex, size_t len);
}

/* Default option set mirrors the upstream dalvikvm defaults. */
struct DalvikOptions {
    std::string boot_class_path;
    std::string class_path;
    std::string main_class;
    std::vector<std::string> main_args;
    int         stack_size_kb;
    int         heap_size_mb;
    bool        verbose_gc;
    bool        jit_enabled;
    bool        help;
};

static void print_help(const char *prog) {
    std::printf(
        "Usage: %s [options] class [args...]\n"
        "Options:\n"
        "  -cp <path>          classpath (colon-separated .dex/.jar files)\n"
        "  -bootcp <path>      boot classpath\n"
        "  -Xss<size>          thread stack size (e.g. -Xss512k)\n"
        "  -Xmx<size>          max heap size (e.g. -Xmx128m)\n"
        "  -verbose:gc         enable GC logging\n"
        "  -Xint               interpreter only (JIT off)\n"
        "  -h, --help          print this help\n",
        prog);
}

static int parse_size_kb(const char *s) {
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (!end || end == s) return -1;
    switch (*end) {
        case 'k': case 'K': return (int)v;
        case 'm': case 'M': return (int)(v * 1024);
        case 'g': case 'G': return (int)(v * 1024 * 1024);
        case 0:             return (int)v;
        default:            return -1;
    }
}

static bool parse_opts(int argc, char **argv, DalvikOptions &out) {
    out.stack_size_kb = 32;
    out.heap_size_mb  = 16;
    out.verbose_gc    = false;
    out.jit_enabled   = true;
    out.help          = false;

    int i = 1;
    while (i < argc) {
        const char *a = argv[i];
        if (!std::strcmp(a, "-h") || !std::strcmp(a, "--help")) {
            out.help = true; return true;
        } else if (!std::strcmp(a, "-cp") && i + 1 < argc) {
            out.class_path = argv[++i];
        } else if (!std::strcmp(a, "-bootcp") && i + 1 < argc) {
            out.boot_class_path = argv[++i];
        } else if (!std::strncmp(a, "-Xss", 4)) {
            int kb = parse_size_kb(a + 4);
            if (kb > 0) out.stack_size_kb = kb;
        } else if (!std::strncmp(a, "-Xmx", 4)) {
            int kb = parse_size_kb(a + 4);
            if (kb > 0) out.heap_size_mb = kb / 1024;
        } else if (!std::strcmp(a, "-verbose:gc")) {
            out.verbose_gc = true;
        } else if (!std::strcmp(a, "-Xint")) {
            out.jit_enabled = false;
        } else if (a[0] == '-') {
            /* Unknown option: ignore (matches upstream's lenient parsing). */
        } else {
            out.main_class = a;
            for (int j = i + 1; j < argc; j++) out.main_args.push_back(argv[j]);
            return true;
        }
        i++;
    }
    return false;
}

/* Load every .dex/.jar on the classpath into the class linker. */
static status_t load_classpath(const std::string &cp) {
    if (cp.empty()) return OK;
    size_t start = 0;
    while (start <= cp.size()) {
        size_t end = cp.find(':', start);
        std::string entry = (end == std::string::npos)
            ? cp.substr(start) : cp.substr(start, end - start);
        if (!entry.empty()) {
            /* Sandbox: synthesize an empty class definition so the linker
             * has something to index. */
            std::string cls = "L" + entry + ";";
            status_t s = ClassLinkerDefineClass(cls.c_str(), nullptr, 0);
            if (s != OK && s != ALREADY_EXISTS) return s;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return OK;
}

extern "C" status_t DalvikvmMain(int argc, char **argv) {
    DalvikOptions opt;
    if (!parse_opts(argc, argv, opt)) {
        std::fprintf(stderr, "dalvikvm: no main class specified\n");
        return BAD_VALUE;
    }
    if (opt.help || opt.main_class.empty()) {
        print_help(argv[0]);
        return opt.help ? OK : BAD_VALUE;
    }

    /* Build the synthetic argv that ArtRuntimeStart expects: keep the
     * original -X flags, drop the main class name and its args. */
    std::vector<char *> vm_argv;
    vm_argv.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], opt.main_class.c_str())) break;
        vm_argv.push_back(argv[i]);
    }
    vm_argv.push_back(nullptr);

    status_t s = ArtRuntimeStart((int)vm_argv.size() - 1, vm_argv.data());
    if (s != OK) {
        std::fprintf(stderr, "dalvikvm: ArtRuntimeStart failed: %d\n", s);
        return s;
    }

    s = load_classpath(opt.class_path);
    if (s != OK) {
        std::fprintf(stderr, "dalvikvm: classpath load failed: %d\n", s);
        ArtRuntimeShutdown();
        return s;
    }

    /* Look up main; in the sandbox the class won't actually contain a
     * runnable main, but the lookup must succeed so callers can validate
     * the launch path. We register a synthetic entry above. */
    std::string main_descriptor = "L" + opt.main_class + ";";
    void *klass = nullptr;
    s = ClassLinkerLookupClass(main_descriptor.c_str(), &klass);
    if (s != OK) {
        std::fprintf(stderr, "dalvikvm: class '%s' not found\n", opt.main_class.c_str());
        ArtRuntimeShutdown();
        return NAME_NOT_FOUND;
    }

    /* Invoke main (interpreter loop is a no-op stub here). */
    ArtRuntimeShutdown();
    return OK;
}

/* C-callable entry for the AfriOS launcher. */
extern "C" int DalvikvmEntry(int argc, char **argv) {
    return (int)DalvikvmMain(argc, argv);
}
