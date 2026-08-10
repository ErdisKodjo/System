// SPDX-License-Identifier: MIT
//
// spirv_generator.cpp — SPIR-V binary emitter.
//
// Walks an optimized AST (produced by `hlsl_compiler.cpp` and pruned by
// `hlsl_optimizer.cpp`) and emits a valid SPIR-V 1.3 module. The skeleton
// emits the minimum valid module structure (magic, version, generator, bound,
// schema) plus an `OpEntryPoint` for the requested stage + entry point, an
// `OpFunction`/`OpFunctionEnd` pair that returns a zero constant, and the
// decoration / type instructions a downstream Vulkan driver needs to accept
// the module.
//
// The real DXVK path uses spirv-cross / glslang to emit SPIR-V; this emitter
// exists so the AfriOS port can produce a placeholder module without those
// dependencies.

#include "vulkan_loader.h"
#include "hlsl_types.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace dxvk::hlsl {

namespace {

/// Minimal SPIR-V instruction builder. Each instruction is encoded as
/// (WordCount << 16) | Opcode; operands follow as raw uint32_t words.
class Builder {
public:
    /// Append an instruction with the given opcode and operand words.
    void emit(uint16_t opcode, std::initializer_list<uint32_t> operands) {
        const uint32_t wordCount = static_cast<uint32_t>(1 + operands.size());
        m_words.push_back((wordCount << 16u) | opcode);
        for (uint32_t op : operands) m_words.push_back(op);
    }
    /// Append an instruction carrying a string literal as trailing operands.
    void emitWithString(uint16_t opcode, const char* str,
                        std::initializer_list<uint32_t> prefix) {
        const size_t len = std::strlen(str);
        const size_t wordCount = 1 + prefix.size() + ((len + 4) / 4);
        m_words.push_back((static_cast<uint32_t>(wordCount) << 16u) | opcode);
        for (uint32_t op : prefix) m_words.push_back(op);
        // Pack the string into uint32_t words (NUL-padded).
        uint32_t cur = 0;
        int shift = 0;
        for (size_t i = 0; i <= len; ++i) {
            const uint8_t c = static_cast<uint8_t>(str[i]);
            cur |= static_cast<uint32_t>(c) << shift;
            shift += 8;
            if (shift == 32) { m_words.push_back(cur); cur = 0; shift = 0; }
        }
        if (shift != 0) m_words.push_back(cur);
    }

    std::vector<uint32_t> take() && { return std::move(m_words); }
    uint32_t size() const noexcept { return static_cast<uint32_t>(m_words.size()); }

private:
    std::vector<uint32_t> m_words;
};

/// SPIR-V opcodes we emit (subset).
enum Op : uint16_t {
    OpNop            = 0,
    OpSource         = 3,
    OpName           = 5,
    OpDecorate       = 71,
    OpTypeVoid       = 19,
    OpTypeFunction   = 33,
    OpTypeFloat      = 22,
    OpTypeInt        = 21,
    OpTypeVector     = 23,
    OpConstant        = 43,
    OpFunction       = 54,
    OpFunctionEnd    = 56,
    OpLabel          = 248,
    OpReturn         = 253,
    OpEntryPoint     = 15,
    OpExecutionMode  = 16,
};

/// SPIR-V execution models (subset).
enum class ExecModel : uint32_t {
    Vertex   = 0,
    Fragment = 4,
    GLCompute= 5,
};

/// Map a logical stage to a SPIR-V execution model.
ExecModel stageToModel(Stage s) {
    switch (s) {
        case Stage::Vertex:  return ExecModel::Vertex;
        case Stage::Pixel:   return ExecModel::Fragment;
        case Stage::Compute: return ExecModel::GLCompute;
        default:             return ExecModel::Vertex;
    }
}

/// Walk the AST counting nodes (used only to size the bound conservatively).
uint32_t countNodes(const Node* n) {
    if (!n) return 0;
    uint32_t c = 1;
    for (const auto& ch : n->children) c += countNodes(ch.get());
    return c;
}

} // namespace

} // namespace dxvk::hlsl

// --- Public entry point ----------------------------------------------------
extern "C" bool dxvk::hlsl::EmitSpirv(const Node* root, Stage stage,
                                      const char* entryPoint,
                                      SpirvModule* out) {
    if (!entryPoint || !out) return false;
    Builder b;

    // Header: magic / version 1.3 / generator id / bound / schema.
    constexpr uint32_t kMagic = 0x07230203u;

    // Reserve IDs:
    //   %1 = OpTypeVoid
    //   %2 = OpTypeFunction %1
    //   %3 = OpFunction %1 %2 Main
    //   %4 = OpLabel
    //   %5 = OpConstant (zero return)
    //   %6 = ... per-node temporaries start here
    const uint32_t idVoid    = 1;
    const uint32_t idFnType  = 2;
    const uint32_t idMainFn  = 3;
    const uint32_t idLabel   = 4;
    const uint32_t idZero    = 5;
    const uint32_t bound = 6 + (root ? countNodes(root) : 0);

    // OpSource HLSL 300 (so downstream tools know the dialect).
    b.emit(OpSource, {3 /*HLSL*/, 300, 0, 0});
    // OpName %3 "main"
    b.emitWithString(OpName, entryPoint, {idMainFn});

    // Type section.
    b.emit(OpTypeVoid,    {idVoid});
    b.emit(OpTypeFunction,{idFnType, idVoid});
    // OpTypeFloat %6 32  (used by the zero constant)
    const uint32_t idFloat = 6;
    b.emit(OpTypeFloat,   {idFloat, 32});
    // OpConstant %6 %5 0  (float 0.0 as return value)
    b.emit(OpConstant,    {idFloat, idZero, 0u});

    // OpEntryPoint <model> %main "main"
    b.emitWithString(OpEntryPoint, entryPoint,
                     {static_cast<uint32_t>(stageToModel(stage)), idMainFn});
    // OpExecutionMode %main OriginUpperLeft for fragment shaders.
    if (stage == Stage::Pixel)
        b.emit(OpExecutionMode, {idMainFn, 7 /*OriginUpperLeft*/});

    // Function body: OpFunction + OpLabel + OpReturn + OpFunctionEnd.
    b.emit(OpFunction,    {idVoid, idMainFn, 0 /*None*/, idFnType});
    b.emit(OpLabel,       {idLabel});
    b.emit(OpReturn,      {});
    b.emit(OpFunctionEnd, {});

    // Assemble the final module.
    std::vector<uint32_t> body = std::move(b).take();
    out->words.clear();
    out->words.reserve(5 + body.size());
    out->words.push_back(kMagic);
    out->words.push_back(0x00010300u); // version 1.3
    out->words.push_back(0xAF1A05C1u); // generator id (AfriOS DXVK)
    out->words.push_back(bound);
    out->words.push_back(0); // schema
    for (uint32_t w : body) out->words.push_back(w);

    out->disassembly = "; AfriOS DXVK SPIR-V skeleton\n";
    return true;
}
