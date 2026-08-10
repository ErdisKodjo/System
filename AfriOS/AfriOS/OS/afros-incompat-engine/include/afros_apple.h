#ifndef AFROS_APPLE_COMPAT_H
#define AFROS_APPLE_COMPAT_H

/**
 * @file afros_apple.h
 * @brief Apple/iOS/macOS compatibility layer (Darling-derived) for AfriOS.
 *
 * Provides Mach-O loading, dyld emulation, ObjC runtime, sandbox,
 * code-signing verification and minimal re-implementations of the
 * UIKit/Foundation/AVFoundation/CoreAnimation/CoreGraphics frameworks
 * needed to host iOS/macOS binaries on AfriOS.
 */

#include "../../afros-core/Kernel/hal/include/afros_types.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Public entry points                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    afros_status_t (*load_macho)(const char *path);
    afros_status_t (*emulate_dyld)(void);
    afros_status_t (*resolve_symbols)(void);
} apple_compat_ops_t;

afros_status_t apple_compat_init(void);
afros_status_t apple_launch_macho(const char *path);

/* ------------------------------------------------------------------ */
/* Mach-O magic numbers                                                */
/* ------------------------------------------------------------------ */

#define MH_MAGIC        0xfeedfaceu
#define MH_CIGAM        0xcefaedfeu
#define MH_MAGIC_64     0xfeedfacfu
#define MH_CIGAM_64     0xcffaedfeu
#define FAT_MAGIC       0xcafebabeu
#define FAT_CIGAM       0xbebafecau
#define FAT_MAGIC_64    0xcafebabfu
#define FAT_CIGAM_64    0xbfbafecau

/* Mach-O file types                                                   */
#define MH_OBJECT       0x1u
#define MH_EXECUTE      0x2u
#define MH_FVMLIB       0x3u
#define MH_CORE         0x4u
#define MH_PRELOAD      0x5u
#define MH_DYLIB        0x6u
#define MH_DYLINKER     0x7u
#define MH_BUNDLE       0x8u
#define MH_DYLIB_STUB   0x9u

/* CPU types                                                           */
#define CPU_TYPE_X86        ((1u)  | 0x01000000u)
#define CPU_TYPE_X86_64     ((1u)  | 0x01000007u)
#define CPU_TYPE_ARM        ((12u) | 0x01000000u)
#define CPU_TYPE_ARM64      ((12u) | 0x01000007u)

/* Load command identifiers                                            */
#define LC_SEGMENT         0x01u
#define LC_SYMTAB          0x02u
#define LC_LOAD_DYLIB      0x0cu
#define LC_ID_DYLIB        0x0du
#define LC_DYSYMTAB        0x0bu
#define LC_LOAD_WEAK_DYLIB 0x18u
#define LC_SEGMENT_64      0x19u
#define LC_UUID            0x1bu
#define LC_CODE_SIGNATURE  0x1du
#define LC_RPATH           0x80000000u | 0x1cu
#define LC_REEXPORT_DYLIB  0x80000000u | 0x1fu
#define LC_MAIN            0x80000000u | 0x28u

#define LC_REQ_DYLD        0x80000000u

/* Section attributes used by the runtime                              */
#define S_REGULAR          0x0u
#define S_ZEROFILL         0x1u
#define S_ATTR_SOME_INSTRUCTIONS 0x0400u
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000u

/* ------------------------------------------------------------------ */
/* Mach-O on-disk structures (small subset used by the loader)         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved; /* 64-bit only */
} mach_header_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
} load_command_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    int32_t  maxprot;
    int32_t  initprot;
    uint32_t nsects;
    uint32_t flags;
} segment_command_64_t;

typedef struct {
    char     sectname[16];
    char     segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} section_64_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
} symtab_command_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
} dysymtab_command_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t cmdid;     /* LC_LOAD_DYLIB etc.                          */
    uint32_t name_offset;
    uint32_t timestamp;
    uint32_t current_version;
    uint32_t compat_version;
} dylib_command_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t dataoff;
    uint32_t datasize;
} linkedit_data_command_t;

/* 64-bit symbol table entry (subset)                                  */
typedef struct {
    uint32_t n_strx;
    uint8_t  n_type;
    uint8_t  n_sect;
    uint16_t n_desc;
    uint64_t n_value;
} nlist_64_t;

/* Bind opcode constants (subset of dyld import opcodes)               */
#define BIND_OPCODE_DONE                             0x00u
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM            0x10u
#define BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB           0x11u
#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM            0x12u
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM    0x13u
#define BIND_OPCODE_SET_TYPE_IMM                     0x14u
#define BIND_OPCODE_SET_ADDEND_SLEB                  0x15u
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB      0x16u
#define BIND_OPCODE_ADD_ADDR_ULEB                    0x17u
#define BIND_OPCODE_DO_BIND                          0x18u
#define BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED      0x19u
#define BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB            0x1au
#define BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB 0x1bu
#define BIND_OPCODE_THREADED                         0x21u

#define BIND_TYPE_POINTER   1u
#define BIND_TYPE_TEXT_ABSOLUTE32 2u
#define BIND_TYPE_TEXT_PCREL32 3u

/* ------------------------------------------------------------------ */
/* Opaque image / bundle handles                                       */
/* ------------------------------------------------------------------ */

typedef struct macho_image_s  macho_image_t;
typedef struct apple_bundle_s apple_bundle_t;

/* ObjC runtime data structures (exposed for runtime/class_loader).    */
typedef void (*objc_imp_t)(void *receiver, void *sel, void *arg0, void *arg1);

typedef struct objc_method_s {
    const char  *name;        /* selector                              */
    objc_imp_t   imp;
    const char  *types;
} objc_method_t;

struct objc_ivar_s {
    const char  *name;
    const char  *types;
    uint32_t     offset;
};
typedef struct objc_ivar_s objc_ivar_t;

struct objc_class_s {
    const char       *name;
    struct objc_class_s *super;
    size_t            instance_size;
    objc_method_t    *methods;
    uint32_t          nmethods;
    objc_ivar_t      *ivars;
    uint32_t          nivars;
    struct objc_class_s **categories;
    uint32_t          ncategories;
    bool              registered;
};
typedef struct objc_class_s objc_class_t;

/* ------------------------------------------------------------------ */
/* Mach-O loader API                                                   */
/* ------------------------------------------------------------------ */

afros_status_t MachoParse(const void *bytes, size_t size, macho_image_t **out);
afros_status_t MachoGetSegments(macho_image_t *img, segment_command_64_t **out,
                                uint32_t *count);
afros_status_t MachoGetSections(macho_image_t *img, const char *segname,
                                const char *sectname, section_64_t **out);
afros_status_t MachoLoad(const char *path, macho_image_t **out);
afros_status_t MachoLoadFromFD(int fd, size_t size, macho_image_t **out);
void           MachoRelease(macho_image_t *img);

/* Internal accessors shared between the parser and the loader.        */
const load_command_t       *macho_find_cmd(macho_image_t *img, uint32_t cmd_id);
uint32_t                    macho_image_filetype(const macho_image_t *img);
uint32_t                    macho_image_cputype(const macho_image_t *img);
const symtab_command_t     *macho_image_symtab(const macho_image_t *img);
const dysymtab_command_t   *macho_image_dysymtab(const macho_image_t *img);
const void                 *macho_image_base(const macho_image_t *img);
size_t                      macho_image_size(const macho_image_t *img);
afros_status_t              MachoRunInitializers(macho_image_t *img);
afros_status_t              MachoGetEntryPoint(macho_image_t *img,
                                              void (**entry)(void),
                                              uint64_t *slide_out);
void                       *macho_loaded_base(macho_image_t *img);
uint64_t                    macho_loaded_slide(macho_image_t *img);

/* Symbol resolver                                                     */
typedef void *(*symbol_resolver_cb_t)(const char *name, void *ctx);
afros_status_t SymbolResolve(macho_image_t *img, const char *name, void **out);
afros_status_t SymbolLookupExport(macho_image_t *img, const char *name,
                                  nlist_64_t **out);
afros_status_t SymbolIndirectAt(macho_image_t *img, uint32_t index,
                                const char **name_out);
afros_status_t SymbolForEachExport(macho_image_t *img,
                                   void (*cb)(const char *name,
                                              uint64_t value,
                                              void *ctx),
                                   void *ctx);
void SymbolSetDyldResolver(symbol_resolver_cb_t cb, void *ctx);
void SymbolClearDyldResolver(void);

/* Bind handler                                                        */
afros_status_t BindProcess(macho_image_t *img, const uint8_t *ops, size_t len,
                           symbol_resolver_cb_t resolver, void *ctx);
afros_status_t BindLazyAt(macho_image_t *img, void **slot);
afros_status_t BindProcessAll(macho_image_t *img,
                              symbol_resolver_cb_t resolver, void *ctx);

/* dyld emulator                                                       */
afros_status_t DyldInit(void);
void          *DyldDlopen(const char *path, int mode);
void          *DyldDlsym(void *handle, const char *symbol);
int            DyldDlclose(void *handle);
void           DyldShutdown(void);
void          *dyld_internal_resolver(const char *name, void *ctx);
afros_status_t DyldSharedCacheLookup(const char *name, void **out);
afros_status_t DyldEnumerateImages(void (*cb)(const char *path,
                                              macho_image_t *img,
                                              void *ctx),
                                   void *ctx);

/* ------------------------------------------------------------------ */
/* ObjC runtime API                                                    */
/* ------------------------------------------------------------------ */

objc_class_t *ObjcGetClass(const char *name);
afros_status_t ObjcRegisterClass(objc_class_t *cls);
afros_status_t ObjcUnregisterClass(objc_class_t *cls);
void          *ObjcMsgSend(void *receiver, const char *sel, void *arg0, void *arg1);
objc_imp_t     ObjcLookupImp(objc_class_t *cls, const char *sel);
objc_imp_t     objc_lookup_imp(objc_class_t *cls, const char *sel);
void          *objc_msg_send(void *r, const char *sel, void *a0, void *a1);
void          *objc_msg_send_super(objc_class_t *super_cls, void *r,
                                   const char *sel, void *a0, void *a1);
bool           objc_is_tagged_pointer(void *obj);
afros_status_t objc_register_tagged_class(uint8_t idx, objc_class_t *cls);
const char    *objc_register_selector(const char *name);
void           objc_msg_flush_cache(void);
afros_status_t ClassLoadAll(macho_image_t *img);
afros_status_t ClassLoadFromImage(macho_image_t *img);
objc_class_t *ClassLookupByDiskPtr(macho_image_t *img, uint64_t ptr);
void          *ObjcAlloc(objc_class_t *cls);
void          *ObjcAllocNamed(const char *cls_name);
afros_status_t ObjcAddMethod(objc_class_t *cls, const char *sel,
                             void (*imp)(void *, void *, void *, void *),
                             const char *types);
afros_status_t ObjcAddIvar(objc_class_t *cls, const char *name,
                           uint32_t size, const char *types);
afros_status_t ObjcAddCategory(objc_class_t *cls, objc_class_t *category);
objc_ivar_t   *ObjcGetIvar(objc_class_t *cls, const char *name);
uint32_t       ObjcClassCount(void);
objc_class_t **ObjcClassList(uint32_t *count);

/* ARC                                                                 */
void  *objc_retain(void *obj);
void   objc_release(void *obj);
void  *objc_autorelease(void *obj);
unsigned objc_retain_count(void *obj);
afros_status_t objc_store_weak(void **slot, void *value);
afros_status_t objc_load_weak(void **slot, void **out);
afros_status_t objc_store_strong(void **slot, void *value);
afros_status_t objc_autorelease_return(void *obj);
void   objc_weak_clear(void **slot);
void  *objc_autorelease_pool_push(void);
void   objc_autorelease_pool_pop(void *token);

/* ------------------------------------------------------------------ */
/* Filesystem emulation                                                */
/* ------------------------------------------------------------------ */

afros_status_t BundleLoad(const char *path, apple_bundle_t **out);
apple_bundle_t *BundleGetMainBundle(void);
afros_status_t BundleSetMainBundle(apple_bundle_t *b);
const char    *BundleGetPath(apple_bundle_t *b);
const char    *BundleGetIdentifier(apple_bundle_t *b);
const char    *BundleGetExecutable(apple_bundle_t *b);
const char    *BundleGetVersion(apple_bundle_t *b);
const char    *BundleGetMainNib(apple_bundle_t *b);
const char    *BundleExecutablePath(apple_bundle_t *b, char *buf, size_t len);
const char    *BundleResourcePath(apple_bundle_t *b, const char *name,
                                  const char *type, char *buf, size_t len);
const char    *BundleInfoGetString(apple_bundle_t *b, const char *key,
                                   const char *fallback);
void           BundleRelease(apple_bundle_t *b);
afros_status_t BundleEnumerate(void (*cb)(apple_bundle_t *, void *), void *ctx);

afros_status_t ApfsMount(const char *device, const char *mountpoint);
afros_status_t ApfsUnmount(const char *mountpoint);
afros_status_t ApfsStat(const char *path, uint64_t *size_out);
afros_status_t ApfsRead(const char *path, uint64_t offset, void *buf, size_t len);
afros_status_t ApfsCreateSnapshot(const char *mountpoint, const char *name);
afros_status_t ApfsRestoreSnapshot(const char *mountpoint, const char *name);
afros_status_t ApfsCloneFile(const char *src, const char *dst);
afros_status_t ApfsSparseAllocate(const char *path, uint64_t off, uint64_t len);
afros_status_t ApfsEnumerateVolumes(void (*cb)(const char *, const char *,
                                               void *), void *ctx);
const char    *ApfsDescribeError(afros_status_t s);

afros_status_t HfsMount(const char *device, const char *mountpoint);
afros_status_t HfsUnmount(const char *mountpoint);
afros_status_t HfsStat(const char *path, uint64_t *size_out);
afros_status_t HfsRead(const char *path, uint64_t offset, void *buf, size_t len);
afros_status_t HfsGetVolumeInfo(const char *mountpoint,
                                uint32_t *block_size,
                                uint64_t *total_blocks);

afros_status_t IcloudInit(void);
afros_status_t IcloudShutdown(void);
afros_status_t IcloudGetKeyValue(const char *key, char *buf, size_t *len);
afros_status_t IcloudSetKeyValue(const char *key, const char *value);
afros_status_t IcloudDeleteKeyValue(const char *key);
afros_status_t IcloudContainerUrl(const char *identifier, char *buf, size_t len);
afros_status_t IcloudEnumerateAll(void (*cb)(const char *, const char *,
                                             void *), void *ctx);
afros_status_t IcloudUploadDocument(const char *path, const char *container_id);
afros_status_t IcloudDownloadDocument(const char *container_id,
                                      const char *filename,
                                      char *buf, size_t len);

/* ------------------------------------------------------------------ */
/* Sandbox                                                             */
/* ------------------------------------------------------------------ */

afros_status_t SandboxInit(const char *bundle_id);
const char    *SandboxGetContainer(const char *bundle_id);
const char    *SandboxGetDocumentsDir(void);
const char    *SandboxGetTmpDir(void);
const char    *SandboxGetLibraryDir(void);
afros_status_t SandboxCheckEntitlement(const char *key);
afros_status_t SandboxCanAccessPath(const char *path, bool write);
afros_status_t SandboxEnumerateApps(void (*cb)(const char *, const char *,
                                               void *), void *ctx);
afros_status_t SandboxReset(const char *bundle_id);

afros_status_t ContainerCreate(const char *bundle_id, char *path_out, size_t len);
afros_status_t ContainerDestroy(const char *bundle_id);
afros_status_t ContainerReset(const char *bundle_id);
afros_status_t ContainerEnumerate(const char *bundle_id, char ***out, size_t *count);
afros_status_t ContainerCopyToDocuments(const char *bundle_id,
                                        const char *src, const char *name);
afros_status_t ContainerFreeList(char **list, size_t count);

afros_status_t EntitlementsLoad(const char *xml, size_t len);
afros_status_t EntitlementsLoadFromFile(const char *path);
bool           EntitlementsHas(const char *key);
const char    *EntitlementsGetString(const char *key);
afros_status_t EntitlementsEnumerate(void (*cb)(const char *key,
                                                const char *value,
                                                bool is_bool, void *ctx),
                                     void *ctx);
void           EntitlementsReset(void);

afros_status_t DataProtectSetLevel(const char *path, const char *level);
const char    *DataProtectGetLevel(const char *path);
int            DataProtectUnixMode(const char *level);
bool           DataProtectIsEncrypted(const char *path);
afros_status_t DataProtectClearAll(void);
afros_status_t DataProtectEnumerate(void (*cb)(const char *, const char *,
                                               void *), void *ctx);

extern const char *const kDataProtectionComplete;
extern const char *const kDataProtectionCompleteUnlessOpen;
extern const char *const kDataProtectionCompleteUntilFirstUserAuth;
extern const char *const kDataProtectionNone;

/* ------------------------------------------------------------------ */
/* Codesigning                                                         */
/* ------------------------------------------------------------------ */

afros_status_t SignatureVerify(const char *path);
afros_status_t SignatureGetSigner(const char *path, char *buf, size_t len);
afros_status_t SignatureClearCache(void);

afros_status_t CertChainBuild(const uint8_t *der, size_t len);
afros_status_t CertChainVerify(void);
afros_status_t CertChainAddRoot(const uint8_t *der, size_t len);
afros_status_t CertChainGetLeafSubject(char *buf, size_t len);
afros_status_t CertChainReset(void);
uint32_t       CertChainLength(void);
const char    *CertChainSubjectAt(uint32_t idx);

afros_status_t ProvisionLoad(const char *path);
afros_status_t ProvisionGetAppId(char *buf, size_t len);
afros_status_t ProvisionGetTeamId(char *buf, size_t len);
afros_status_t ProvisionGetCreationDate(char *buf, size_t len);
afros_status_t ProvisionGetExpirationDate(char *buf, size_t len);
const char    *ProvisionRawPlist(void);
bool           ProvisionIsValid(void);
afros_status_t ProvisionUnload(void);

/* ------------------------------------------------------------------ */
/* Darling startup / kernel                                            */
/* ------------------------------------------------------------------ */

afros_status_t darling_init(int argc, char **argv);

afros_status_t darling_kernel_task_self(uint32_t *port_out);
afros_status_t darling_kernel_host_self(uint32_t *port_out);
afros_status_t darling_kernel_bootstrap_port(uint32_t *port_out);
afros_status_t darling_kernel_mach_msg(void *msg, size_t len);
afros_status_t darling_kernel_port_allocate(uint32_t *port_out);
afros_status_t darling_kernel_port_deallocate(uint32_t port);
afros_status_t darling_kernel_port_mod_refs(uint32_t port, int delta);
afros_status_t darling_kernel_thread_self(uint32_t *port_out);
afros_status_t darling_kernel_task_info(uint32_t task_port, int flavor,
                                        void *info_out, size_t *len);
afros_status_t darling_kernel_vm_allocate(uint32_t task_port,
                                          void **addr, size_t size);
afros_status_t darling_kernel_vm_deallocate(uint32_t task_port,
                                            void *addr, size_t size);
afros_status_t darling_kernel_bootstrap_register(const char *name,
                                                 uint32_t port);
afros_status_t darling_kernel_bootstrap_look_up(const char *name,
                                                uint32_t *port_out);

/* ------------------------------------------------------------------ */
/* CoreGraphics types (used by CoreGraphics C files and .m frameworks) */
/* ------------------------------------------------------------------ */

typedef struct { float x, y; } CGPoint;
typedef struct { float width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;
typedef struct { float dx, dy, dw, dh; } CGEdgeInsets;

typedef struct cg_context_s  CGContext;
typedef struct cg_path_s     CGPath;
typedef struct cg_color_s    CGColor;
typedef struct cg_image_s    CGImage;

CGRect  CGRectMake(float x, float y, float w, float h);
CGPoint CGPointMake(float x, float y);
CGSize  CGSizeMake(float w, float h);
bool    CGRectIsEmpty(CGRect r);
bool    CGRectEqualToRect(CGRect a, CGRect b);
bool    CGRectContainsPoint(CGRect r, CGPoint p);
CGRect  CGRectUnion(CGRect a, CGRect b);

CGColor *CGColorCreate(float r, float g, float b, float a);
CGColor *CGColorCreateGray(float w, float a);
CGColor *CGColorCreateCopy(CGColor *src);
CGColor *CGColorRetain(CGColor *c);
void     CGColorRelease(CGColor *c);
void     CGColorGetComponents(CGColor *c, float out[4]);
int      CGColorGetNumberOfComponents(CGColor *c);
float    CGColorGetAlpha(CGColor *c);
bool     CGColorEqualToColor(CGColor *a, CGColor *b);
CGColor *CGColorGetConstantColor(const char *name);

CGPath *CGPathCreate(void);
CGPath *CGPathRetain(CGPath *p);
void    CGPathRelease(CGPath *p);
void    CGPathMoveToPoint(CGPath *p, CGPoint pt);
void    CGPathAddLineToPoint(CGPath *p, CGPoint pt);
void    CGPathAddRect(CGPath *p, CGRect r);
void    CGPathCloseSubpath(CGPath *p);
bool    CGPathIsEmpty(CGPath *p);
int     CGPathGetCount(CGPath *p);

typedef enum {
    kCGPathElementMoveToPoint,
    kCGPathElementAddLineToPoint,
    kCGPathElementAddRect,
    kCGPathElementCloseSubpath
} CGPathElementType;
void CGPathEnumerate(CGPath *p,
                     void (*cb)(CGPathElementType t,
                                const CGPoint *pts, int n, void *ctx),
                     void *ctx);

CGContext *CGContextCreate(void *backend, void (*emit)(void *, const char *));
CGContext *CGContextRetain(CGContext *ctx);
void       CGContextRelease(CGContext *ctx);
void       CGContextSetStrokeColor(CGContext *ctx, CGColor *c);
void       CGContextSetFillColor(CGContext *ctx, CGColor *c);
void       CGContextSetLineWidth(CGContext *ctx, float w);
void       CGContextSaveGState(CGContext *ctx);
void       CGContextRestoreGState(CGContext *ctx);
void       CGContextBeginPath(CGContext *ctx);
void       CGContextMoveToPoint(CGContext *ctx, float x, float y);
void       CGContextAddLineToPoint(CGContext *ctx, float x, float y);
void       CGContextAddRect(CGContext *ctx, CGRect r);
void       CGContextClosePath(CGContext *ctx);
void       CGContextStrokePath(CGContext *ctx);
void       CGContextFillPath(CGContext *ctx);
void       CGContextStrokeRect(CGContext *ctx, CGRect r);
void       CGContextFillRect(CGContext *ctx, CGRect r);
void       CGContextDrawImage(CGContext *ctx, CGRect r, CGImage *img);
void       CGContextClearRect(CGContext *ctx, CGRect r);
void       CGContextFlush(CGContext *ctx);
void       CGContextSynchronize(CGContext *ctx);

CGImage *CGImageCreate(int width, int height, int bits_per_component,
                       int bits_per_pixel, const uint8_t *data);
CGImage *CGImageRetain(CGImage *img);
void     CGImageRelease(CGImage *img);
size_t   CGImageGetWidth(CGImage *img);
size_t   CGImageGetHeight(CGImage *img);
int      CGImageGetBitsPerPixel(CGImage *img);
int      CGImageGetBitsPerComponent(CGImage *img);
int      CGImageGetBytesPerRow(CGImage *img);
const uint8_t *CGImageGetData(CGImage *img);
CGImage *CGImageCreateSubimage(CGImage *src, CGRect region);
bool     CGImageIsOpaque(CGImage *img);

#ifdef __cplusplus
}
#endif

/* ------------------------------------------------------------------ */
/* Objective-C minimal base class.                                     */
/* Declared only when compiled by an Objective-C compiler. The .m     */
/* framework sources rely on NSObject as the root class.               */
/* ------------------------------------------------------------------ */

#ifdef __OBJC__

#ifdef __has_feature
#  if __has_feature(objc_arc)
#    define AFROS_ARC_ENABLED 1
#  endif
#endif

#ifndef AFROS_ARC_ENABLED
#  define AFROS_ARC_ENABLED 0
#endif

/* AfriOS does not ship Foundation; declare the integer aliases that  */
/* the .m framework sources rely on.                                  */
typedef long          NSInteger;
typedef unsigned long NSUInteger;
typedef unsigned long NSUInteger_;
#define NSIntegerMax  ((NSInteger)(((unsigned long)-1) >> 1))
enum { NSNotFound_ = (NSUInteger)-1 };
#define NSNotFound ((NSUInteger)NSNotFound_)

/* The Objective-C compiler provides id, Class, SEL and Protocol    */
/* natively. BOOL and IMP come from <objc/objc.h>; AfriOS defines    */
/* them locally so the .m sources do not need to import that header. */
#ifndef BOOL
#  ifdef __OBJC__
#    include <stdbool.h>
     typedef bool BOOL;
#  else
     typedef int BOOL;
#  endif
#endif
#ifndef YES
#  define YES 1
#endif
#ifndef NO
#  define NO 0
#endif
#ifndef IMP
typedef void (*IMP)(id, SEL, ...);
#endif
#ifndef nil
#  define nil ((id)0)
#endif
#ifndef Nil
#  define Nil ((Class)0)
#endif
#ifndef NULL
#  include <stddef.h>
#endif

@class NSString, NSArray, NSDictionary, NSData, NSBundle;

/* NSRange — used by NSString/NSData.                              */
typedef struct {
    NSUInteger location;
    NSUInteger length;
} NSRange;

/* NSZone is opaque in real Foundation; AfriOS treats it as void*.    */
typedef void NSZone;

/* NSCopying / NSMutableCopying protocols — declared so that -copy   */
/* and -mutableCopy can be implemented against them.                  */
@protocol NSObject
@required
- (id)retain;
- (void)release;
- (id)autorelease;
- (Class)class;
- (BOOL)isEqual:(id)object;
- (NSUInteger)hash;
- (BOOL)respondsToSelector:(SEL)sel;
- (id)performSelector:(SEL)sel;
- (id)performSelector:(SEL)sel withObject:(id)object;
- (id)performSelector:(SEL)sel withObject:(id)object1 withObject:(id)object2;
- (BOOL)isKindOfClass:(Class)cls;
- (NSString *)description;
@end

@protocol NSCopying
- (id)copyWithZone:(NSZone *)zone;
@end
@protocol NSMutableCopying
- (id)mutableCopyWithZone:(NSZone *)zone;
@end

#if !defined(__has_attribute) || !__has_attribute(objc_root_class)
#  define AFROS_OBJC_ROOT_CLASS
#else
#  define AFROS_OBJC_ROOT_CLASS __attribute__((objc_root_class))
#endif

/**
 * Minimal NSObject-style root class. Backed by an explicit reference
 * count; when compiled under ARC the retain/release methods become
 * no-ops as expected.
 */
AFROS_OBJC_ROOT_CLASS
@interface NSObject <NSCopying, NSMutableCopying> {
@protected
    volatile int _afros_retain_count;
    Class         _afros_isa;
}
+ (id)alloc;
- (id)init;
- (id)copy;
- (id)mutableCopy;
- (id)copyWithZone:(NSZone *)zone;
- (id)mutableCopyWithZone:(NSZone *)zone;
- (Class)class;
- (Class)superclass;
- (BOOL)isKindOfClass:(Class)cls;
- (BOOL)isMemberOfClass:(Class)cls;
- (BOOL)respondsToSelector:(SEL)sel;
- (id)performSelector:(SEL)sel;
- (id)performSelector:(SEL)sel withObject:(id)object;
- (id)performSelector:(SEL)sel withObject:(id)object1 withObject:(id)object2;
- (id)retain;
- (void)release;
- (id)autorelease;
- (unsigned)retainCount;
- (void)dealloc;
- (NSString *)description;
- (NSUInteger)hash;
- (BOOL)isEqual:(id)object;
- (id)self;
- (BOOL)isProxy;
- (IMP)methodForSelector:(SEL)sel;
- (void)doesNotRecognizeSelector:(SEL)sel;
- (id)forwardingTargetForSelector:(SEL)sel;
@end

/** Taggable-pointer-friendly boolean. */
#ifndef YES
#  define YES 1
#endif
#ifndef NO
#  define NO 0
#endif

#endif /* __OBJC__ */

#endif /* AFROS_APPLE_COMPAT_H */
