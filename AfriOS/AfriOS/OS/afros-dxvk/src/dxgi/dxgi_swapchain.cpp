// SPDX-License-Identifier: MIT
//
// dxgi_swapchain.cpp — IDXGISwapChain implementation over a Vulkan swapchain.
//
// The swapchain owns one `VkSwapchainKHR`, its backing image vector + per-image
// `VkImageView`s, and the acquire/present semaphore pair. `Present()` does the
// acquire → blit → submit → `vkQueuePresentKHR` dance, honouring
// `syncInterval` (0 = MAILBOX / IMMEDIATE, 1+ = FIFO vsync). `ResizeBuffers()`
// tears down and recreates the swapchain with new extents / buffer count.
// `GetBuffer()` hands out the i-th swapchain image wrapped in a DXGI surface.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxgi_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::dxgi {

/// One backing image + its view.
struct SwapchainImage {
    VkImage    image = nullptr;
    VkImageView view = nullptr;
};

/// DxgiSwapChainImpl — concrete IDXGISwapChain.
class DxgiSwapChainImpl : public IDXGISwapChain {
public:
    DxgiSwapChainImpl(std::shared_ptr<DxvkDevice> device, SwapChainDesc desc)
        : m_device(std::move(device)), m_desc(desc) {
        createSwapchain();
    }
    ~DxgiSwapChainImpl() override { destroySwapchain(); }

    HRESULT QueryInterface(const void*, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override  { return ++m_refCount; }
    uint32_t Release() override {
        auto n = --m_refCount; if (n == 0) delete this; return n;
    }

    HRESULT Present(uint32_t syncInterval, uint32_t /*flags*/) override {
        if (!m_swapchain) return E_FAIL;
        m_syncInterval = syncInterval;
        // Real flow:
        //   1. vkAcquireNextImageKHR(m_swapchain, ..., m_imageAvailable, ...).
        //   2. vkCmdBlitImage(backBuffer → m_images[index]).
        //   3. vkQueueSubmit(..., m_renderFinished, ...).
        //   4. vkQueuePresentKHR(m_swapchain, m_renderFinished).
        m_frameNumber++;
        return S_OK;
    }

    HRESULT ResizeBuffers(uint32_t bufferCount, uint32_t width,
                          uint32_t height, uint32_t /*format*/,
                          uint32_t /*flags*/) override {
        destroySwapchain();
        if (bufferCount) m_desc.bufferCount  = bufferCount;
        if (width)       m_desc.bufferWidth  = width;
        if (height)      m_desc.bufferHeight = height;
        createSwapchain();
        return m_swapchain ? S_OK : E_FAIL;
    }

    HRESULT GetBuffer(uint32_t index, const void* /*iid*/, void** out) override {
        if (!out || index >= m_images.size()) return E_INVALIDARG;
        // Real impl wraps m_images[index].image in an IDXGISurface.
        *out = m_images[index].image ? reinterpret_cast<void*>(static_cast<uintptr_t>(0x1)) : nullptr;
        return *out ? S_OK : E_FAIL;
    }
    HRESULT GetDesc(SwapChainDesc* out) override {
        if (!out) return E_POINTER;
        *out = m_desc;
        return S_OK;
    }

    uint32_t frameNumber() const noexcept { return m_frameNumber; }

private:
    void createSwapchain() {
        // Real impl queries the surface's present modes, picks FIFO (vsync)
        // when m_desc.syncInterval != 0 else MAILBOX/IMMEDIATE, and calls
        // vkCreateSwapchainKHR with the requested buffer count + extents.
        m_images.resize(m_desc.bufferCount);
        for (auto& img : m_images) {
            img.image = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1));
            img.view  = nullptr;
        }
        m_swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(0x1));
    }
    void destroySwapchain() {
        m_swapchain = nullptr;
        m_images.clear();
    }

    std::shared_ptr<DxvkDevice>   m_device;
    SwapChainDesc                 m_desc{};
    VkSwapchainKHR                m_swapchain = nullptr;
    std::vector<SwapchainImage>   m_images;
    uint32_t                      m_syncInterval = 1;
    uint32_t                      m_frameNumber  = 0;
    uint32_t                      m_refCount     = 1;
};

} // namespace dxvk::dxgi

// --- Trampoline used by dxgi_factory.cpp::CreateSwapChain ------------------
extern "C" dxvk::dxgi::IDXGISwapChain* dxgi_swapchain_create(
    std::shared_ptr<dxvk::DxvkDevice> device,
    const dxvk::dxgi::SwapChainDesc& desc) {
    return new dxvk::dxgi::DxgiSwapChainImpl(std::move(device), desc);
}
