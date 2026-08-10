/*
 * compiler/dex_to_native.cc — Direct DEX bytecode → native code fast path.
 *
 * The optimizing compiler is heavy (SSA, register allocation, codegen) and
 * is only worth running on hot methods. For cold methods, ART has a fast
 * path that walks the DEX bytecode once and emits native code in a single
 * pass — no IR, no optimizations. This is the "quicken" filter.
 *
 * This module implements that fast path. It walks a small set of opcodes
 * (const, move, return, invoke-virtual, iget/iput), emits a one-line stub
 * for each, and writes the resulting native page. Cold-start latency in
 * the sandbox is dominated by the number of methods we touch, so keeping
 * this O(method_size) is the whole point.
 */

#include "android_sandbox.h"
#include "android_sandbox_defs.h"

#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

/* Subset of DEX opcodes we recognise. */
enum DexOpcode : uint8_t {
    OP_NOP             = 0x00,
    OP_MOVE            = 0x01,
    OP_CONST_4         = 0x12,
    OP_CONST_16        = 0x13,
    OP_RETURN_VOID     = 0x0e,
    OP_RETURN          = 0x0f,
    OP_RETURN_WIDE     = 0x10,
    OP_RETURN_OBJECT   = 0x11,
    OP_INVOKE_VIRTUAL  = 0x6e,
    OP_INVOKE_STATIC   = 0x71,
    OP_IGET            = 0x52,
    OP_IPUT            = 0x59,
    OP_ADD_INT         = 0x90,
};

struct DexInst {
    DexOpcode op;
    uint8_t   reg_a;
    uint8_t   reg_b;
    int32_t   imm;
};

/* Single-pass bytecode decoder — recognises only the opcodes above. */
static size_t decode(const uint8_t *code, size_t len, std::vector<DexInst> &out) {
    size_t i = 0;
    while (i + 1 < len) {
        DexInst d{};
        d.op = (DexOpcode)code[i];
        uint8_t unit = code[i + 1];
        d.reg_a = (unit >> 4) & 0xf;
        d.reg_b = unit & 0xf;
        switch (d.op) {
        case OP_NOP:
            i += 2; break;
        case OP_MOVE:
        case OP_ADD_INT:
            i += 2; break;
        case OP_CONST_4:
            d.imm = (int8_t)((unit & 0xf) | ((unit & 0x8) ? 0xf0 : 0));
            i += 2; break;
        case OP_CONST_16:
            if (i + 3 >= len) return i;
            d.imm = (int16_t)(code[i + 2] | (code[i + 3] << 8));
            i += 4; break;
        case OP_RETURN_VOID:
            i += 2; break;
        case OP_RETURN:
        case OP_RETURN_WIDE:
        case OP_RETURN_OBJECT:
            i += 2; break;
        case OP_INVOKE_VIRTUAL:
        case OP_INVOKE_STATIC:
            if (i + 5 >= len) return i;
            d.imm = code[i + 2] | (code[i + 3] << 8);
            i += 6; break;
        case OP_IGET:
        case OP_IPUT:
            if (i + 3 >= len) return i;
            d.imm = code[i + 2] | (code[i + 3] << 8);
            i += 4; break;
        default:
            /* Unknown opcode — stop decoding. */
            return i;
        }
        out.push_back(d);
    }
    return i;
}

/* Emit a tiny stub: returns 0 (matches the optimizing compiler stub). */
static status_t emit(const std::vector<DexInst> &insts, void **out_code) {
    (void)insts;
    constexpr size_t kPage = 4096;
    void *page = mmap(nullptr, kPage, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) return NO_MEMORY;
    unsigned char ret_x86 = 0xc3;
    unsigned int  ret_arm = 0xd65f03c0;
    std::memcpy(page, &ret_x86, 1);
    std::memcpy((char *)page + 4, &ret_arm, 4);
    if (out_code) *out_code = page;
    return OK;
}

class DexToNative {
public:
    status_t Compile(const char *cls, const char *method,
                     const char *sig, void **out_code) {
        if (!cls || !method) return BAD_VALUE;
        /* Sandbox: no actual bytecode is available; pretend we have a
         * one-instruction `return-void` body. */
        uint8_t body[2] = { OP_RETURN_VOID, 0 };
        std::vector<DexInst> insts;
        decode(body, sizeof(body), insts);
        stats_.compiled++;
        stats_.instructions += insts.size();
        return emit(insts, out_code);
    }
    size_t compiled()     const { return stats_.compiled.load(); }
    size_t instructions() const { return stats_.instructions.load(); }
private:
    struct Stats {
        std::atomic<size_t> compiled{0};
        std::atomic<size_t> instructions{0};
    } stats_;
};

static std::mutex g_lock;
static DexToNative *g_d2n = nullptr;
static DexToNative *d2n() {
    std::lock_guard<std::mutex> lk(g_lock);
    if (!g_d2n) g_d2n = new DexToNative();
    return g_d2n;
}

extern "C" status_t DexToNativeCompile(const char *cls, const char *method,
                                       const char *sig, void **out_code) {
    return d2n()->Compile(cls, method, sig, out_code);
}

extern "C" size_t DexToNativeCompiledCount(void) { return d2n()->compiled(); }
extern "C" size_t DexToNativeInstructionCount(void) { return d2n()->instructions(); }
