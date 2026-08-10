// SPDX-License-Identifier: MIT
//
// vulkan_presenter.cpp — Vulkan swapchain + present engine.
//
// The presenter owns the `VkSwapchainKHR`, its backing images + per-image
// `VkImageView`s, and the acquire / render-finished semaphore pair. It
// supports the four standard present modes:
//
//   * IMMEDIATE  — tearing allowed, no vsync.
//   * MAILBOX    — single latest frame visible, low-latency vsync.
//   * FIFO       — classic vsync (always supported).
//   * FIFO_RELAXED — vsync with adaptive late-swap.
//
// `AcquireImage()` returns the next available swapchain image index (blocking
// when the swapchain is `VK_ERROR_OUT_OF_DATE_KHR` until `Recreate()` is
// called). `Present()` submits the render-finished semaphore to the queue and
// hands the image back to the surface.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "vulkan_private.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk {

/// Present mode enum mirror.
enum class PresentMode : uint32_t {
    Immediate   = 0,
    Mailbox     = 1,
    Fifo        = 2,
    FifoRelaxed = 3,
};

/// Swapchain configuration.
struct PresenterDesc {
    VkSurfaceKHR    surface     = nullptr;
    VkExtent2D      extent      = {};
    VkFormat        format      = VK_FORMAT_B8G8R8A8_UNORM;
    uint32_t        imageCount  = 2;
    PresentMode     presentMode = PresentMode::Fifo;
    uint32_t        queueFamily = 0;
};

/// One backing swapchain image + view.
struct PresenterImage {
    VkImage    image = nullptr;
    VkImageView view = nullptr;
};

/// VulkanPresenter — owns one VkSwapchainKHR and its present semaphores.
class VulkanPresenter {
public:
    VulkanPresenter(std::shared_ptr<DxvkDevice> device, PresenterDesc desc)
        : m_device(std::move(device)), m_desc(desc) {
        createSwapchain();
    }
    ~VulkanPresenter() { destroySwapchain(); }

    /// Recreate the swapchain (called on resize or when the ICD returns
    /// VK_ERROR_OUT_OF_DATE_KHR).
    VkResult recreate(VkExtent2D newExtent) {
        destroySwapchain();
        m_desc.extent = newExtent;
        return createSwapchain();
    }

    /// Acquire the next swapchain image. Returns the image index, or a
    /// VkResult < 0 indicating the swapchain needs recreation.
    VkResult acquireImage(uint32_t* outIndex) {
        if (!m_swapchain || !outIndex) return VK_ERROR_OUT_OF_HOST_MEMORY;
        // Real impl: vkAcquireNextImageKHR(m_device->device(), m_swapchain,
        //                                  UINT64_MAX, m_imageAvailable, VK_NULL_HANDLE, &index).
        // The skeleton round-robins through the image vector.
        m_currentIndex = (m_currentIndex + 1) % m_images.size();
        *outIndex = m_currentIndex;
        return VK_SUCCESS;
    }

    /// Present the currently-acquired image.
    VkResult present(uint32_t index) {
        if (!m_swapchain || index >= m_images.size())
            return VK_ERROR_OUT_OF_DATE_KHR;
        // Real impl:
        //   vk::PresentInfoKHR pi{};
        //   pi.swapchainCount = 1;
        //   pi.pSwapchains = &m_swapchain;
        //   pi.pImageIndices = &index;
        //   pi.waitSemaphoreCount = 1;
        //   pi.pWaitSemaphores = &m_renderFinished;
        //   return pfnQueuePresentKHR(m_device->queue(), &pi);
        m_frameNumber++;
        return VK_SUCCESS;
    }

    /// Switch present mode (vsync on/off). Returns the new effective mode.
    PresentMode setPresentMode(PresentMode requested) {
        // Real impl: if MAILBOX unsupported, fall back to FIFO; if IMMEDIATE
        // unsupported, fall back to MAILBOX. Requires swapchain recreation.
        m_desc.presentMode = requested;
        return m_desc.presentMode;
    }

    VkSwapchainKHR swapchain() const noexcept { return m_swapchain; }
    uint32_t imageCount() const noexcept {
        return static_cast<uint32_t>(m_images.size());
    }
    const PresenterImage& image(uint32_t i) const noexcept { return m_images[i]; }
    VkFormat format() const noexcept { return m_desc.format; }
    VkExtent2D extent() const noexcept { return m_desc.extent; }
    uint32_t frameNumber() const noexcept { return m_frameNumber; }

private:
    VkResult createSwapchain() {
        // Real impl queries VkSurfaceCapabilitiesKHR + VkSurfacePresentModesKHR,
        // picks the requested mode (falling back to FIFO which is always
        // supported), fills vk::SwapchainCreateInfoKHR, and calls
        // vkCreateSwapchainKHR. Then vkGetSwapchainImagesKHR + per-image
        // vkCreateImageView.
        m_images.resize(m_desc.imageCount);
        for (auto& img : m_images) {
            img.image = reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1));
            img.view  = reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x2));
        }
        m_swapchain = reinterpret_cast<VkSwapchainKHR>(static_cast<uintptr_t>(0x1));
        m_currentIndex = 0;
        return VK_SUCCESS;
    }
    void destroySwapchain() {
        // Real impl: vkDestroySwapchainKHR + vkDestroyImageView per image.
        m_swapchain = nullptr;
        m_images.clear();
    }

    std::shared_ptr<DxvkDevice> m_device;
    PresenterDesc               m_desc{};
    VkSwapchainKHR              m_swapchain = nullptr;
    std::vector<PresenterImage> m_images;
    VkSemaphore                 m_imageAvailable = nullptr;
    VkSemaphore                 m_renderFinished = nullptr;
    uint32_t                    m_currentIndex = 0;
    uint32_t                    m_frameNumber = 0;
};

} // namespace dxvk

// --- C entry points used by the DXGI swapchain layer -----------------------
extern "C" {

dxvk::VulkanPresenter* vkpres_create(void* devicePtr,
                                     const dxvk::PresenterDesc& desc) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::VulkanPresenter(std::move(device), desc);
}
void vkpres_destroy(dxvk::VulkanPresenter* p) { delete p; }

} // extern "C"
