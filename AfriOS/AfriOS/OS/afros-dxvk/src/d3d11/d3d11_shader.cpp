// SPDX-License-Identifier: MIT
//
// d3d11_shader.cpp — D3D11 shader-blob → SPIR-V translator.
//
// D3D11 receives shaders either as pre-compiled DXBC bytecodes (the common
// case on Windows) or as HLSL source compiled via D3DCompile. The real DXVK
// path lowers DXBC → SPIR-V via `dxbc_compiler`; this skeleton routes source
// compilation through the shared `hlsl::CompileHlslToSpirv` front-end and
// treats pre-compiled DXBC as an opaque blob that we wrap in a `VkShaderModule`
// after a (stub) translation pass.
//
// The compiled module is wrapped in a `D3D11ShaderImpl` that exposes the
// appropriate COM interface (ID3D11VertexShader / ID3D11PixelShader) and holds
// the `VkShaderModule` handle for the device-context bind path.

#include "vulkan_loader.h"
#include "dxvk_device.h"
#include "d3d11_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::d3d11 {

/// D3D11ShaderImpl — concrete shader wrapper. Templated on the COM interface
/// it presents (ID3D11VertexShader / ID3D11PixelShader); we keep one shared
/// implementation here and let d3d11_device.cpp cast to the right interface.
class D3D11ShaderImpl : public ID3D11DeviceChild {
public:
    D3D11ShaderImpl(std::shared_ptr<DxvkDevice> device, ShaderStage stage,
                    const void* bytecode, size_t byteLength)
        : m_device(std::move(device)), m_stage(stage)
        , m_byteLength(byteLength) {
        if (byteLength && bytecode) {
            m_bytecode.resize(byteLength);
            std::memcpy(m_bytecode.data(), bytecode, byteLength);
        }
        compileAndUpload();
    }
    ~D3D11ShaderImpl() override {
        if (m_module) m_device->destroyShaderModule(m_module);
    }

    HRESULT QueryInterface(const void* /*iid*/, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override  { return ++m_refCount; }
    uint32_t Release() override {
        auto n = --m_refCount;
        if (n == 0) delete this;
        return n;
    }

    VkShaderModule module() const noexcept { return m_module; }
    ShaderStage    stage()  const noexcept { return m_stage; }
    const std::vector<uint8_t>& bytecode() const noexcept { return m_bytecode; }

private:
    void compileAndUpload() {
        // If the bytecode already looks like SPIR-V magic, upload it directly.
        // Otherwise treat as DXBC and route through the hlsl front-end (stub).
        const bool isSpirv = m_bytecode.size() >= 4 &&
            *reinterpret_cast<const uint32_t*>(m_bytecode.data()) == 0x07230203u;
        if (isSpirv) {
            dxvk::ShaderModuleDesc d{};
            d.code  = reinterpret_cast<const uint32_t*>(m_bytecode.data());
            d.words = m_bytecode.size() / sizeof(uint32_t);
            m_module = m_device->createShaderModule(d);
            return;
        }
        // DXBC → SPIR-V: in the skeleton we emit a minimal no-op module so the
        // device has a handle to bind. The real path lowers DXBC instructions
        // through the shared hlsl::spirv_generator.
        std::vector<uint32_t> spirv = {0x07230203u, 0x00010300u, 0u, 1u, 0u};
        dxvk::ShaderModuleDesc d{};
        d.code  = spirv.data();
        d.words = spirv.size();
        m_module = m_device->createShaderModule(d);
    }

    std::shared_ptr<DxvkDevice> m_device;
    ShaderStage                 m_stage = ShaderStage::Unknown;
    std::vector<uint8_t>        m_bytecode;
    size_t                      m_byteLength = 0;
    VkShaderModule              m_module = nullptr;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk::d3d11

// --- Trampoline used by d3d11_device.cpp::CreateVertexShader/... -----------
extern "C" void* d3d11_shader_create(void* devicePtr,
                                     const dxvk::d3d11::ShaderDesc& desc) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    auto* impl = new dxvk::d3d11::D3D11ShaderImpl(
        std::move(device), desc.stage, desc.bytecode, desc.byteLength);
    return static_cast<dxvk::d3d11::ID3D11DeviceChild*>(impl);
}
