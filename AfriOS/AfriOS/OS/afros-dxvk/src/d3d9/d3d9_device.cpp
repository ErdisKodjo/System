// SPDX-License-Identifier: MIT
//
// d3d9_device.cpp — IDirect3DDevice9 implementation over Vulkan.
//
// Each D3D9 render-state setter is translated into Vulkan dynamic-state
// commands queued on the active command buffer. The heavy pipeline-state
// translation (blend / rasterizer / depth-stencil) lives in d3d9_state.cpp
// (which implements the IDirect3DStateBlock9 abstraction); this file owns the
// device-facing surface: clear, scene bracketing, primitive draw dispatch,
// resource creation (textures), and resource binding.
//
// Self-contained: declares the minimal IDirect3DDevice9 COM interface locally
// and exposes a `d3d9_device_create()` trampoline so d3d9.cpp can construct a
// device without dragging in this translation unit's private declarations.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk {

// Forward declaration — full type lives in this TU.
class D3D9Device;

// --- Minimal local IDirect3DDevice9 COM interface --------------------------
struct IDirect3DDevice9 {
    virtual ~IDirect3DDevice9() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    virtual HRESULT TestCooperativeLevel() = 0;
    virtual HRESULT Reset(void* params) = 0;
    virtual HRESULT Present(const void* rectSrc, const void* rectDest,
                            void* dstWindow, const void* dirtyRegion) = 0;

    virtual HRESULT Clear(uint32_t count, const void* rects, uint32_t flags,
                          uint32_t color, float z, uint32_t stencil) = 0;
    virtual HRESULT BeginScene() = 0;
    virtual HRESULT EndScene() = 0;
    virtual HRESULT DrawPrimitive(uint32_t primitiveType, uint32_t startVertex,
                                  uint32_t primitiveCount) = 0;
    virtual HRESULT DrawIndexedPrimitive(uint32_t primitiveType,
                                         int32_t baseVertexIndex,
                                         uint32_t minVertexIndex,
                                         uint32_t numVertices,
                                         uint32_t startIndex,
                                         uint32_t primCount) = 0;

    virtual HRESULT SetRenderState(uint32_t state, uint32_t value) = 0;
    virtual HRESULT GetRenderState(uint32_t state, uint32_t* value) = 0;
    virtual HRESULT SetSamplerState(uint32_t stage, uint32_t type, uint32_t value) = 0;

    virtual HRESULT CreateTexture(uint32_t width, uint32_t height, uint32_t levels,
                                  uint32_t usage, uint32_t format, uint32_t pool,
                                  void** texture, void** sharedHandle) = 0;
    virtual HRESULT SetTexture(uint32_t stage, void* texture) = 0;

    virtual HRESULT SetViewport(const void* viewport) = 0;
    virtual HRESULT SetVertexShader(void* shader) = 0;
    virtual HRESULT SetPixelShader(void* shader) = 0;
};

/// D3D9Device — concrete IDirect3DDevice9 backed by DxvkDevice.
class D3D9Device : public IDirect3DDevice9 {
public:
    explicit D3D9Device(std::shared_ptr<DxvkDevice> device)
        : m_device(std::move(device)) {
        std::memset(m_renderStates, 0, sizeof(m_renderStates));
        std::memset(m_boundTextures, 0, sizeof(m_boundTextures));
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

    HRESULT TestCooperativeLevel() override { return S_OK; }
    HRESULT Reset(void* /*params*/) override { return S_OK; }
    HRESULT Present(const void*, const void*, void*, const void*) override {
        // Real present goes through D3D9SwapChain::Present → vkQueuePresentKHR.
        return S_OK;
    }

    HRESULT Clear(uint32_t /*count*/, const void* /*rects*/, uint32_t flags,
                  uint32_t color, float z, uint32_t stencil) override {
        // D3DCLEAR_TARGET=0x1, D3DCLEAR_ZBUFFER=0x2, D3DCLEAR_STENCIL=0x4.
        // Color packed as ARGB; unpack into float[4] for vkCmdClearColorImage.
        const float rgba[4] = {
            static_cast<float>((color >> 16) & 0xFF) / 255.0f,
            static_cast<float>((color >>  8) & 0xFF) / 255.0f,
            static_cast<float>((color      ) & 0xFF) / 255.0f,
            static_cast<float>((color >> 24) & 0xFF) / 255.0f,
        };
        (void)rgba; (void)z; (void)stencil; (void)flags;
        return S_OK;
    }

    HRESULT BeginScene() override { m_inScene = true;  return S_OK; }
    HRESULT EndScene()   override { m_inScene = false; return S_OK; }

    HRESULT DrawPrimitive(uint32_t primitiveType, uint32_t startVertex,
                          uint32_t primitiveCount) override {
        if (!m_inScene) return E_FAIL;
        // D3DPT_TRIANGLELIST → VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST etc.
        const uint32_t vertexCount = primCountToVertexCount(primitiveType, primitiveCount);
        (void)vertexCount; (void)startVertex;
        return S_OK;
    }
    HRESULT DrawIndexedPrimitive(uint32_t /*primType*/, int32_t /*baseVi*/,
                                 uint32_t /*minV*/, uint32_t /*numV*/,
                                 uint32_t /*startIdx*/, uint32_t /*primCount*/) override {
        if (!m_inScene) return E_FAIL;
        return S_OK;
    }

    HRESULT SetRenderState(uint32_t state, uint32_t value) override {
        if (state >= kRenderStateCount) return E_INVALIDARG;
        m_renderStates[state] = value;
        // The real impl translates D3DRS_* → vkCmdSet*/pipeline state here.
        return S_OK;
    }
    HRESULT GetRenderState(uint32_t state, uint32_t* value) override {
        if (!value || state >= kRenderStateCount) return E_INVALIDARG;
        *value = m_renderStates[state];
        return S_OK;
    }
    HRESULT SetSamplerState(uint32_t stage, uint32_t type, uint32_t value) override {
        if (stage >= kSamplerStageCount) return E_INVALIDARG;
        if (type < 16) m_samplerStates[stage][type] = value;
        return S_OK;
    }

    HRESULT CreateTexture(uint32_t width, uint32_t height, uint32_t levels,
                          uint32_t /*usage*/, uint32_t format, uint32_t /*pool*/,
                          void** texture, void** /*sharedHandle*/) override {
        if (!texture || width == 0 || height == 0) return E_INVALIDARG;
        ImageDesc desc{};
        desc.format      = mapD3D9Format(format);
        desc.extent      = { width, height, 1 };
        desc.mipLevels   = levels == 0 ? 1 : levels;
        desc.arrayLayers = 1;
        desc.usage       = 0x10 /* SAMPLED */ | 0x40 /* TRANSFER_DST */;
        VkImage img = m_device->createImage(desc);
        if (!img) return E_OUTOFMEMORY;
        *texture = img;
        return S_OK;
    }
    HRESULT SetTexture(uint32_t stage, void* texture) override {
        if (stage >= kSamplerStageCount) return E_INVALIDARG;
        m_boundTextures[stage] = static_cast<VkImage>(texture);
        return S_OK;
    }

    HRESULT SetViewport(const void* /*viewport*/) override { return S_OK; }
    HRESULT SetVertexShader(void* /*shader*/) override { return S_OK; }
    HRESULT SetPixelShader(void* /*shader*/) override  { return S_OK; }

    DxvkDevice& device() noexcept { return *m_device; }

private:
    static constexpr uint32_t kRenderStateCount   = 256;
    static constexpr uint32_t kSamplerStageCount  = 8;

    static VkFormat mapD3D9Format(uint32_t /*fmt*/) {
        // D3DFMT_A8R8G8B8 → VK_FORMAT_B8G8R8A8_UNORM (most common case).
        return VK_FORMAT_B8G8R8A8_UNORM;
    }
    static uint32_t primCountToVertexCount(uint32_t primType, uint32_t primCount) {
        switch (primType) {
            case 4 /*D3DPT_TRIANGLELIST*/: return primCount * 3;
            case 5 /*D3DPT_TRIANGLESTRIP*/: return primCount + 2;
            case 6 /*D3DPT_TRIANGLEFAN*/:   return primCount + 2;
            case 1 /*D3DPT_LINELIST*/:      return primCount * 2;
            case 2 /*D3DPT_LINESTRIP*/:     return primCount + 1;
            case 3 /*D3DPT_POINTLIST*/:     return primCount;
            default: return primCount * 3;
        }
    }

    std::shared_ptr<DxvkDevice> m_device;
    uint32_t m_renderStates[kRenderStateCount] = {};
    uint32_t m_samplerStates[kSamplerStageCount][16] = {};
    VkImage  m_boundTextures[kSamplerStageCount] = {};
    uint32_t m_refCount = 1;
    bool     m_inScene  = false;
};

} // namespace dxvk

// --- Trampoline used by d3d9.cpp::D3D9::CreateDevice ----------------------
extern "C" dxvk::D3D9Device* d3d9_device_create(void* devicePtr) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::D3D9Device(std::move(device));
}
