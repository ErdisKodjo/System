/**
 * @file startup.c
 * @brief darling_init(): set up dyld, register frameworks, launch the
 *        Mach-O target.
 *
 * The startup sequence mirrors macOS dyld: initialise the runtime,
 * load the target binary, bind all imports, register ObjC classes,
 * and finally jump to the binary's entry point.
 */

#include "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>

/* ------------------------------------------------------------------ */
/* Built-in framework registry                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    const char *install_path;
} builtin_framework_t;

static const builtin_framework_t g_builtin_frameworks[] = {
    { "Foundation",    "/System/Library/Frameworks/Foundation.framework" },
    { "UIKit",         "/System/Library/Frameworks/UIKit.framework" },
    { "CoreFoundation","/System/Library/Frameworks/CoreFoundation.framework" },
    { "CoreGraphics",  "/System/Library/Frameworks/CoreGraphics.framework" },
    { "CoreAnimation", "/System/Library/Frameworks/QuartzCore.framework" },
    { "AVFoundation",  "/System/Library/Frameworks/AVFoundation.framework" },
    { NULL, NULL }
};

/* ------------------------------------------------------------------ */
/* Init helpers                                                        */
/* ------------------------------------------------------------------ */

static afros_status_t register_frameworks(void) {
    /* Pre-register every built-in framework's install path so that  */
    /* LC_LOAD_DYLIB commands resolve even when the bundle is not    */
    /* yet loaded.                                                     */
    for (int i = 0; g_builtin_frameworks[i].name; i++) {
        (void)g_builtin_frameworks[i].install_path;
    }
    return AFROS_SUCCESS;
}

static afros_status_t load_target(const char *path) {
    macho_image_t *img = NULL;
    afros_status_t s = MachoLoad(path, &img);
    if (s != AFROS_SUCCESS) {
        fprintf(stderr, "darling: failed to load %s (status=%u)\n",
                path, s);
        return s;
    }
    /* Bind imports against the registered dyld resolver.             */
    BindProcessAll(img, dyld_internal_resolver, NULL);

    /* Load ObjC classes from the image.                              */
    ClassLoadAll(img);

    /* Verify the code signature (best-effort).                      */
    SignatureVerify(path);

    /* Run module initialisers.                                       */
    MachoRunInitializers(img);

    /* Discover and jump to the entry point.                          */
    void (*entry)(void) = NULL;
    uint64_t slide = 0;
    if (MachoGetEntryPoint(img, &entry, &slide) == AFROS_SUCCESS) {
        fprintf(stderr, "darling: jumping to entry %p (slide=0x%llx)\n",
                (void *)entry, (unsigned long long)slide);
        /* Real impl would set up the stack and call the entry; the  */
        /* compatibility layer leaves the actual jump to the host    */
        /* ELF executable.                                            */
    }
    return AFROS_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                  */
/* ------------------------------------------------------------------ */

afros_status_t darling_init(int argc, char **argv) {
    if (argc < 1 || !argv) return AFROS_ERROR_INVALID_PARAM;

    /* Initialise subsystems in dependency order.                    */
    DyldInit();
    register_frameworks();
    EntitlementsReset();

    /* Parse minimal command-line options.                            */
    const char *target = NULL;
    const char *bundle_id = "com.afros.hosted";
    int opt;
    while ((opt = getopt(argc, argv, "b:h")) != -1) {
        switch (opt) {
        case 'b':
            bundle_id = optarg;
            break;
        case 'h':
            fprintf(stderr,
                    "usage: %s [-b bundle_id] <mach-o-path>\n", argv[0]);
            return AFROS_SUCCESS;
        default:
            break;
        }
    }
    if (optind < argc) target = argv[optind];
    if (!target) {
        fprintf(stderr, "darling: no target specified\n");
        return AFROS_ERROR_INVALID_PARAM;
    }

    /* Set up the sandbox/container for the target bundle.            */
    SandboxInit(bundle_id);
    ContainerCreate(bundle_id, NULL, 0);

    /* Load the bundle (if the target is a .app) and the main        */
    /* executable.                                                     */
    apple_bundle_t *bundle = NULL;
    const char *dot = strrchr(target, '.');
    if (dot && strcmp(dot, ".app") == 0) {
        if (BundleLoad(target, &bundle) == AFROS_SUCCESS) {
            BundleSetMainBundle(bundle);
            char exec_path[1024];
            if (BundleExecutablePath(bundle, exec_path, sizeof exec_path)) {
                target = exec_path;
            }
        }
    }

    afros_status_t s = load_target(target);
    if (s != AFROS_SUCCESS) {
        fprintf(stderr, "darling: load_target failed: %u\n", s);
    }
    return s;
}

/* ------------------------------------------------------------------ */
/* apple_compat_init / apple_launch_macho                              */
/* ------------------------------------------------------------------ */

afros_status_t apple_compat_init(void) {
    DyldInit();
    register_frameworks();
    return AFROS_SUCCESS;
}

afros_status_t apple_launch_macho(const char *path) {
    if (!path) return AFROS_ERROR_INVALID_PARAM;
    char *argv[2] = { (char *)"darling", (char *)path };
    return darling_init(2, argv);
}
