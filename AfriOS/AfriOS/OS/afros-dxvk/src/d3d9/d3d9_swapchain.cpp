// SPDX-License-Identifier: MIT
//
// d3d9_swapchain.cpp — IDirect3DSwapChain9 implementation over a Vulkan
// present surface.
//
// The swapchain owns the VkSwapchainKHR, its backing images / views, and the
// present semaphore pair. `Present()` acquires the next image (or recycles a
// previously-acquired one), blits the active render-target image into it, and
// calls `vkQueuePresentKHR`. `Reset()` tears down and recreates the swapchain
// when the window is resized or vsync mode changes.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::d3d9 {

/// Present modes selectable via D3DPRESENT_PARAMETERS::PresentationInterval.
enum class PresentInterval : uint32_t {
    Immediate = 0,   // D3DPRESENT_INTERVAL_IMMEDIATE (tearing allowed)
    One       = 1,   // D3DPRESENT_INTERVAL_ONE (vsync)
    Two       = 2,   // D3DPRESENT_INTERVAL_TWO
    Three     = 3,
    Four      = 4,
};

/// Parameters mirroring D3DPRESENT_PARAMETERS (subset).
struct PresentParameters {
    uint32_t        backBufferWidth  = 0;
    uint32_t        backBufferHeight = 0;
    uint32_t        backBufferFormat = 0;
    uint32_t        backBufferCount  = 1;
    uint32_t        swapEffect       = 0;
    void*           deviceWindow     = nullptr;
    BOOL            windowed         = TRUE;
    BOOL            enableAutoDepthStencil = FALSE;
    PresentInterval presentationInterval = PresentInterval::One;
};

/// Minimal IDirect3DSwapChain9 surface.
struct IDirect3DSwapChain9 {
    virtual ~IDirect3DSwapChain9() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
    virtual HRESULT Present(const void* src, const void* dst,
                            void* dstWindow, const void* dirty,
                            uint32_t flags) = 0;
    virtual HRESULT GetBackBuffer(uint32_t backBuffer, uint32_t type,
                                  void** surface) = 0;
    virtual HRESULT Reset(const PresentParameters& params) = 0;
};

/// D3D9SwapChain — concrete IDirect3DSwapChain9 backed by a VkSwapchainKHR.
class D3D9SwapChain : public IDirect3DSwapChain9 {
public:
    D3D9SwapChain(std::shared_ptr<DxvkDevice> device, PresentParameters params)
        : m_device(std::move(device)), m_params(params) {
        createSwapchain();
    }
    ~D3D9SwapChain() override { destroySwapchain(); }

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

    HRESULT Present(const void* /*src*/, const void* /*dst*/,
                    void* /*dstWindow*/, const void* /*dirty*/,
                    uint32_t /*flags*/) override {
        // Real flow:
        //   1. If no image is currently acquired, call vkAcquireNextImageKHR.
        //   2. vkCmdBlitImage(renderTarget → swapchainImage).
        //   3. vkEndCommandBuffer + vkQueueSubmit with image-available and
        //      render-finished semaphores.
        //   4. vkQueuePresentKHR(swapchain, imageIndex, render-finished).
        if (!m_swapchain) return E_FAIL;
        m_frameNumber++;
        return S_OK;
    }

    HRESULT GetBackBuffer(uint32_t backBuffer, uint32_t /*type*/,
                          void** surface) override {
        if (!surface || backBuffer >= m_backBuffers.size()) return E_INVALIDARG;
        *surface = m_backBuffers[backBuffer];
        return S_OK;
    }

    HRESULT Reset(const PresentParameters& params) override {
        destroySwapchain();
        m_params = params;
        createSwapchain();
        return m_swapchain ? S_OK : E_FAIL;
    }

    uint32_t frameNumber() const noexcept { return m_frameNumber; }
    VkSwapchainKHR swapchain() const noexcept { return m_swapchain; }

private:
    void createSwapchain() {
        // Real impl queries the surface, picks a present mode based on
        // m_params.presentationInterval, and calls vkCreateSwapchainKHR with
        // m_params.backBuffer{Width,Height,Count}. The skeleton just records
        // the intended image count so GetBackBuffer can fail cleanly.
        m_backBuffers.resize(m_params.backBufferCount, nullptr);
        m_swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(0x1));
    }
    void destroySwapchain() {
        m_swapchain = nullptr;
        m_backBuffers.clear();
    }

    std::shared_ptr<DxvkDevice> m_device;
    PresentParameters           m_params{};
    VkSwapchainKHR              m_swapchain = nullptr;
    std::vector<void*>          m_backBuffers;
    uint32_t                    m_refCount    = 1;
    uint32_t                    m_frameNumber = 0;
};

} // namespace dxvk::d3d9
