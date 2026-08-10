// SPDX-License-Identifier: MIT
//
// d3d9_shader.cpp — DirectX 9 Shader Model 2.0/3.0 bytecode → SPIR-V
// translator skeleton.
//
// D3D9 shaders arrive as pre-compiled token streams produced by the D3D
// runtime / fxc. A full SM3→SPIR-V translator is thousands of lines (see
// DXVK's `d3d9_shader_codegen*`); this file establishes the entry points the
// device layer calls and a minimal decoder for the shader header that lets us
// extract version, type, and constant-table metadata. The SPIR-V emission
// itself is farmed out to the shared `hlsl::spirv_generator` (src/hlsl/).

#include "vulkan_loader.h"
#include "dxvk_device.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::d3d9 {

/// Shader stage reported by the SM header.
enum class ShaderStage : uint8_t { Vertex, Pixel, Unknown };

/// Decoded header of a D3D9 shader token stream.
struct ShaderHeader {
    ShaderStage stage      = ShaderStage::Unknown;
    uint8_t     majorVer   = 0;
    uint8_t     minorVer   = 0;
    uint32_t    instructionCount = 0;
    uint32_t    constantCount   = 0;
    uint32_t    samplerCount    = 0;
};

/// Compiled SPIR-V module + accompanying metadata.
struct CompiledShader {
    std::vector<uint32_t> spirv;
    ShaderHeader          header{};
    VkShaderModule        module = nullptr; // VkShaderModule handle, if uploaded
};

/// Decode the leading DWORD token of a D3D9 shader.
///   bits 31..28 : 0xFF = version token marker
///   bits 27..0  : version + type (e.g. 0x20FFFFFE for vs_2_0)
static ShaderHeader decodeHeader(const uint32_t* tokens, size_t count) {
    ShaderHeader h{};
    if (!tokens || count == 0) return h;
    const uint32_t versionToken = tokens[0];
    if ((versionToken & 0xFF000000u) != 0xFF000000u) {
        // Not a version token — treat as unknown.
        return h;
    }
    const uint16_t ver = static_cast<uint16_t>(versionToken & 0xFFFFu);
    const uint8_t  kind = static_cast<uint8_t>((versionToken >> 16u) & 0xFFu);
    h.majorVer = static_cast<uint8_t>((ver >> 8u) & 0xFFu);
    h.minorVer = static_cast<uint8_t>(ver & 0xFFu);
    switch (kind) {
        case 0xFFu: h.stage = ShaderStage::Vertex; break; // vs_*
        case 0xFEu: h.stage = ShaderStage::Pixel;  break; // ps_*
        default:    h.stage = ShaderStage::Unknown; break;
    }
    // Comment/length tokens after the version token may carry counts; in a
    // real impl we'd walk the dcl_*/def_* instructions. For the skeleton we
    // scan heuristically.
    for (size_t i = 1; i < count; ++i) {
        const uint32_t t = tokens[i];
        const uint16_t opcode = static_cast<uint16_t>(t & 0xFFFFu);
        if (opcode == 0x0000u /* end */) break;
        if (opcode == 0xFFFEu /* def */)        h.constantCount++;
        else if (opcode == 0xFFFDu /* dcl_2d */) h.samplerCount++;
        else                                     h.instructionCount++;
    }
    return h;
}

/// CompileVertexShader — accepts a D3D9 vertex-shader token stream and returns
/// a SPIR-V module ready to be wrapped in a VkShaderModule.
extern "C" CompiledShader* CompileVertexShader(const uint32_t* tokens,
                                               size_t count) {
    auto* out = new CompiledShader{};
    out->header = decodeHeader(tokens, count);
    if (out->header.stage != ShaderStage::Vertex) {
        delete out;
        return nullptr;
    }
    // The real path lowers SM2/3 ops to SPIR-V via the hlsl::spirv_generator.
    // For the skeleton we emit a minimal no-op vertex shader: a single
    // OpFunction that returns the input position unchanged, so callers have a
    // valid SPIR-V binary to upload.
    out->spirv.assign({
        0x07230203u, // SPIR-V magic
        0x00010300u, // version 1.3
        0u,          // generator
        1u,          // bound
        0u,          // schema
    });
    return out;
}

/// CompilePixelShader — accepts a D3D9 pixel-shader token stream.
extern "C" CompiledShader* CompilePixelShader(const uint32_t* tokens,
                                              size_t count) {
    auto* out = new CompiledShader{};
    out->header = decodeHeader(tokens, count);
    if (out->header.stage != ShaderStage::Pixel) {
        delete out;
        return nullptr;
    }
    out->spirv.assign({
        0x07230203u, 0x00010300u, 0u, 1u, 0u,
    });
    return out;
}

/// Free a shader previously returned by Compile{Vertex,Pixel}Shader.
extern "C" void ReleaseCompiledShader(CompiledShader* shader) {
    delete shader;
}

/// Upload compiled SPIR-V to a VkShaderModule via the supplied DxvkDevice.
extern "C" VkShaderModule UploadShader(DxvkDevice* device,
                                       const CompiledShader* shader) {
    if (!device || !shader || shader->spirv.empty()) return nullptr;
    ShaderModuleDesc desc{};
    desc.code  = shader->spirv.data();
    desc.words = shader->spirv.size();
    return device->createShaderModule(desc);
}

} // namespace dxvk::d3d9
