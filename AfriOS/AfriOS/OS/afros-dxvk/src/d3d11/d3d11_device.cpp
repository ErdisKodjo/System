// SPDX-License-Identifier: MIT
//
// d3d11_device.cpp — ID3D11Device implementation.
//
// The D3D11 device is the top-level factory for buffers, textures, shaders,
// views, and command contexts. Each `Create*` method validates the incoming
// D3D11 descriptor, forwards to the appropriate sibling trampoline
// (d3d11_buffer_create / d3d11_texture_create / d3d11_shader_create defined in
// the other d3d11 translation units) to build the concrete wrapper, and
// returns the COM interface to the caller.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "d3d11_types.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk::d3d11 {

// Forward declarations of COM views we hand out.
struct ID3D11RenderTargetView {
    virtual ~ID3D11RenderTargetView() = default;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
};
struct ID3D11DepthStencilView {
    virtual ~ID3D11DepthStencilView() = default;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
};
struct ID3D11DeviceContext;
class  D3D11DeviceContext;

/// Minimal ID3D11Device COM interface (subset).
struct ID3D11Device {
    virtual ~ID3D11Device() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    virtual HRESULT CreateBuffer(const BufferDesc* desc, const void* initData,
                                 ID3D11Buffer** out) = 0;
    virtual HRESULT CreateTexture2D(const Texture2DDesc* desc,
                                    const void* initData,
                                    ID3D11Texture2D** out) = 0;
    virtual HRESULT CreateVertexShader(const void* bytecode, size_t length,
                                       void* linkage,
                                       void** outShader) = 0;
    virtual HRESULT CreatePixelShader(const void* bytecode, size_t length,
                                      void* linkage,
                                      void** outShader) = 0;
    virtual HRESULT CreateRenderTargetView(ID3D11Resource* res, const void* desc,
                                           ID3D11RenderTargetView** out) = 0;
    virtual HRESULT CreateDepthStencilView(ID3D11Resource* res, const void* desc,
                                           ID3D11DepthStencilView** out) = 0;
    virtual HRESULT CreateInputLayout(const void* elems, uint32_t count,
                                      const void* bytecode, size_t length,
                                      void** out) = 0;
    virtual void GetImmediateContext(D3D11DeviceContext** out) = 0;
};

/// D3D11Device — concrete ID3D11Device.
class D3D11Device : public ID3D11Device {
public:
    explicit D3D11Device(std::shared_ptr<DxvkDevice> device)
        : m_device(std::move(device)) {}

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

    HRESULT CreateBuffer(const BufferDesc* desc, const void* /*initData*/,
                         ID3D11Buffer** out) override {
        if (!desc || !out) return E_INVALIDARG;
        *out = d3d11_buffer_create(&m_device, *desc);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateTexture2D(const Texture2DDesc* desc, const void* /*initData*/,
                            ID3D11Texture2D** out) override {
        if (!desc || !out) return E_INVALIDARG;
        if (desc->width == 0 || desc->height == 0) return E_INVALIDARG;
        *out = d3d11_texture_create(&m_device, *desc);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateVertexShader(const void* bytecode, size_t length,
                                void* /*linkage*/, void** outShader) override {
        if (!bytecode || length == 0 || !outShader) return E_INVALIDARG;
        ShaderDesc d{};
        d.stage      = ShaderStage::Vertex;
        d.bytecode   = bytecode;
        d.byteLength = length;
        *outShader = d3d11_shader_create(&m_device, d);
        return *outShader ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreatePixelShader(const void* bytecode, size_t length,
                               void* /*linkage*/, void** outShader) override {
        if (!bytecode || length == 0 || !outShader) return E_INVALIDARG;
        ShaderDesc d{};
        d.stage      = ShaderStage::Pixel;
        d.bytecode   = bytecode;
        d.byteLength = length;
        *outShader = d3d11_shader_create(&m_device, d);
        return *outShader ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateRenderTargetView(ID3D11Resource* /*res*/, const void* /*desc*/,
                                    ID3D11RenderTargetView** out) override {
        if (!out) return E_POINTER;
        // Real impl: derive VkImageView from the resource + view desc.
        *out = nullptr;
        return E_NOTIMPL;
    }
    HRESULT CreateDepthStencilView(ID3D11Resource* /*res*/, const void* /*desc*/,
                                    ID3D11DepthStencilView** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return E_NOTIMPL;
    }
    HRESULT CreateInputLayout(const void* /*elems*/, uint32_t /*count*/,
                               const void* /*bytecode*/, size_t /*length*/,
                               void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return E_NOTIMPL;
    }
    void GetImmediateContext(D3D11DeviceContext** /*out*/) override {
        // Real impl lazily constructs a D3D11DeviceContext wrapping the
        // DxvkDevice's primary command buffer; omitted here to avoid a
        // circular dependency with d3d11_context.cpp.
    }

    DxvkDevice& device() noexcept { return *m_device; }

private:
    std::shared_ptr<DxvkDevice> m_device;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk::d3d11

// --- C entry point exported to Wine (D3D11CreateDevice) --------------------
extern "C" {

dxvk::HRESULT D3D11CreateDevice(void* /*adapter*/, uint32_t /*driverType*/,
                                void* /*software*/, uint32_t /*flags*/,
                                const void* /*featureLevels*/, uint32_t /*nlevels*/,
                                uint32_t /*sdkVersion*/,
                                dxvk::d3d11::ID3D11Device** outDevice,
                                uint32_t* outFeatureLevel,
                                void** outContext) {
    if (!outDevice) return dxvk::E_INVALIDARG;
    // Real impl picks a DxvkAdapter via DXGI, calls DxvkDevice::create, then
    // constructs D3D11Device. The skeleton returns E_FAIL when no Vulkan
    // device is available so callers can fall back gracefully.
    *outDevice = nullptr;
    if (outFeatureLevel) *outFeatureLevel = 0;
    if (outContext)      *outContext      = nullptr;
    return dxvk::E_FAIL;
}

} // extern "C"
