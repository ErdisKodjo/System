// SPDX-License-Identifier: MIT
//
// d3d11_context.cpp — ID3D11DeviceContext (immediate context) implementation.
//
// The immediate context owns the active Vulkan command buffer. Bind calls
// (VSSetShader / PSSetShader / IASetInputLayout / IASetVertexBuffers /
// OMSetRenderTargets / RSSetViewport) record state into a deferred-state
// vector that is flushed into real vkCmdBind* calls immediately before the
// next Draw / DrawIndexed / Dispatch. Clear* and draw calls write directly
// into the command buffer.

#include "vulkan_loader.h"
#include "dxvk_device.h"
#include "d3d11_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::d3d11 {

// Forward declarations of sibling view interfaces (declared in d3d11_device.cpp).
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
class  D3D11Device;

/// Minimal ID3D11DeviceContext COM interface (subset).
struct ID3D11DeviceContext {
    virtual ~ID3D11DeviceContext() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    virtual void VSSetShader(void* shader, void** instances, uint32_t count) = 0;
    virtual void PSSetShader(void* shader, void** instances, uint32_t count) = 0;
    virtual void IASetInputLayout(void* layout) = 0;
    virtual void IASetVertexBuffers(uint32_t startSlot, uint32_t numBuffers,
                                    ID3D11Buffer** buffers, const uint32_t* strides,
                                    const uint32_t* offsets) = 0;
    virtual void IASetIndexBuffer(ID3D11Buffer* buffer, uint32_t format,
                                  uint32_t offset) = 0;
    virtual void OMSetRenderTargets(uint32_t numViews,
                                    ID3D11RenderTargetView** rts,
                                    ID3D11DepthStencilView* dsv) = 0;
    virtual void RSSetViewports(uint32_t count, const VkViewport* viewports) = 0;
    virtual void RSSetScissorRects(uint32_t count, const VkRect2D* rects) = 0;

    virtual void Draw(uint32_t vertexCount, uint32_t startVertex) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex,
                             int32_t  baseVertex) = 0;
    virtual void DrawInstanced(uint32_t vc, uint32_t ic,
                               uint32_t sv, uint32_t si) = 0;
    virtual void DrawIndexedInstanced(uint32_t ic, uint32_t iic,
                                      uint32_t si, int32_t bv, uint32_t sii) = 0;
    virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;

    virtual void ClearRenderTargetView(ID3D11RenderTargetView* rt,
                                       const float color[4]) = 0;
    virtual void ClearDepthStencilView(ID3D11DepthStencilView* dsv,
                                       uint32_t clearFlags, float depth,
                                       uint8_t stencil) = 0;
    virtual void Flush() = 0;
};

/// D3D11DeviceContext — concrete immediate context.
class D3D11DeviceContextImpl : public ID3D11DeviceContext {
public:
    explicit D3D11DeviceContextImpl(std::shared_ptr<DxvkDevice> device)
        : m_device(std::move(device)) {
        std::memset(m_boundRTs, 0, sizeof(m_boundRTs));
        std::memset(m_boundVBs, 0, sizeof(m_boundVBs));
        std::memset(m_boundStrides, 0, sizeof(m_boundStrides));
        std::memset(m_boundOffsets, 0, sizeof(m_boundOffsets));
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

    void VSSetShader(void* shader, void** /*instances*/, uint32_t /*count*/) override {
        m_vs = shader; m_dirty |= kDirtyPipeline;
    }
    void PSSetShader(void* shader, void** /*instances*/, uint32_t /*count*/) override {
        m_ps = shader; m_dirty |= kDirtyPipeline;
    }
    void IASetInputLayout(void* layout) override {
        m_inputLayout = layout; m_dirty |= kDirtyInputLayout;
    }
    void IASetVertexBuffers(uint32_t startSlot, uint32_t numBuffers,
                            ID3D11Buffer** buffers, const uint32_t* strides,
                            const uint32_t* offsets) override {
        for (uint32_t i = 0; i < numBuffers && startSlot + i < kMaxVBs; ++i) {
            m_boundVBs[startSlot + i]     = buffers ? buffers[i] : nullptr;
            m_boundStrides[startSlot + i] = strides ? strides[i] : 0;
            m_boundOffsets[startSlot + i] = offsets ? offsets[i] : 0;
        }
        m_dirty |= kDirtyVertexBuffers;
    }
    void IASetIndexBuffer(ID3D11Buffer* buffer, uint32_t /*format*/,
                          uint32_t offset) override {
        m_boundIB = buffer; m_indexOffset = offset;
        m_dirty |= kDirtyIndexBuffer;
    }
    void OMSetRenderTargets(uint32_t numViews, ID3D11RenderTargetView** rts,
                            ID3D11DepthStencilView* dsv) override {
        for (uint32_t i = 0; i < numViews && i < kMaxRTs; ++i)
            m_boundRTs[i] = rts ? rts[i] : nullptr;
        for (uint32_t i = numViews; i < kMaxRTs; ++i) m_boundRTs[i] = nullptr;
        m_boundDSV = dsv;
        m_dirty |= kDirtyRenderTargets;
    }
    void RSSetViewports(uint32_t count, const VkViewport* viewports) override {
        m_viewportCount = count < kMaxViewports ? count : kMaxViewports;
        for (uint32_t i = 0; i < m_viewportCount; ++i) m_viewports[i] = viewports[i];
        m_dirty |= kDirtyViewport;
    }
    void RSSetScissorRects(uint32_t count, const VkRect2D* rects) override {
        m_scissorCount = count < kMaxViewports ? count : kMaxViewports;
        for (uint32_t i = 0; i < m_scissorCount; ++i) m_scissors[i] = rects[i];
        m_dirty |= kDirtyScissor;
    }

    void Draw(uint32_t vertexCount, uint32_t startVertex) override {
        flushState();
        // vkCmdDraw(vertexCount, 1, startVertex, 0)
        (void)vertexCount; (void)startVertex;
    }
    void DrawIndexed(uint32_t indexCount, uint32_t startIndex,
                     int32_t baseVertex) override {
        flushState();
        (void)indexCount; (void)startIndex; (void)baseVertex;
    }
    void DrawInstanced(uint32_t vc, uint32_t /*ic*/, uint32_t sv,
                       uint32_t /*si*/) override {
        flushState();
        (void)vc; (void)sv;
    }
    void DrawIndexedInstanced(uint32_t /*ic*/, uint32_t /*iic*/, uint32_t /*si*/,
                              int32_t /*bv*/, uint32_t /*sii*/) override {
        flushState();
    }
    void Dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {
        flushState();
    }

    void ClearRenderTargetView(ID3D11RenderTargetView* /*rt*/,
                               const float /*color*/[4]) override {
        // Real impl: vkCmdClearAttachments / ClearColorImage depending on
        // whether we are inside an active render pass.
    }
    void ClearDepthStencilView(ID3D11DepthStencilView* /*dsv*/,
                               uint32_t /*clearFlags*/, float /*depth*/,
                               uint8_t /*stencil*/) override {}
    void Flush() override {
        // End + submit the command buffer, then begin a fresh one.
        m_device->waitIdle();
    }

private:
    enum DirtyBits : uint32_t {
        kDirtyPipeline       = 1u << 0,
        kDirtyInputLayout    = 1u << 1,
        kDirtyVertexBuffers  = 1u << 2,
        kDirtyIndexBuffer    = 1u << 3,
        kDirtyRenderTargets  = 1u << 4,
        kDirtyViewport       = 1u << 5,
        kDirtyScissor        = 1u << 6,
    };

    void flushState() {
        // In a real impl this is where deferred vkCmdBindPipeline /
        // vkCmdBindVertexBuffers / vkCmdBindDescriptorSets / vkCmdSetViewport
        // calls land. The skeleton just clears the dirty mask.
        m_dirty = 0;
    }

    static constexpr uint32_t kMaxVBs      = 32;
    static constexpr uint32_t kMaxRTs      = 8;
    static constexpr uint32_t kMaxViewports= 16;

    std::shared_ptr<DxvkDevice> m_device;
    void*                m_vs = nullptr;
    void*                m_ps = nullptr;
    void*                m_inputLayout = nullptr;
    ID3D11Buffer*        m_boundVBs[kMaxVBs] = {};
    uint32_t             m_boundStrides[kMaxVBs] = {};
    uint32_t             m_boundOffsets[kMaxVBs] = {};
    ID3D11Buffer*        m_boundIB = nullptr;
    uint32_t             m_indexOffset = 0;
    ID3D11RenderTargetView* m_boundRTs[kMaxRTs] = {};
    ID3D11DepthStencilView* m_boundDSV = nullptr;
    VkViewport           m_viewports[kMaxViewports] = {};
    VkRect2D             m_scissors[kMaxViewports]  = {};
    uint32_t             m_viewportCount = 0;
    uint32_t             m_scissorCount  = 0;
    uint32_t             m_dirty = 0;
    uint32_t             m_refCount = 1;
};

} // namespace dxvk::d3d11
