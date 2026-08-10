// SPDX-License-Identifier: MIT
//
// dxvk_device.h — DxvkDevice, AfriOS's wrapper around a VkDevice + its primary
// graphics/compute queue.
//
// The D3D9/D3D11/D3D12/DXGI translation layers all sit on top of a single
// `DxvkDevice`. The class owns the logical `VkDevice`, the chosen graphics
// queue, the Vulkan loader dispatch table, and exposes high-level factory
// helpers (`createBuffer`, `createImage`, `createPipeline`, ...) that the
// translation layers call instead of touching Vulkan directly. This keeps all
// real `vk*` calls concentrated in `src/vulkan/*.cpp`, so the D3D layers stay
// backend-agnostic and unit-testable.

#pragma once

#include "vulkan_loader.h"
#include "dxvk_adapter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dxvk {

/// Opaque description of a buffer to create.
struct BufferDesc {
    uint64_t size        = 0;
    uint32_t usage       = 0;   // bitmask of VkBufferUsageFlagBits
    uint32_t memoryFlags = 0;   // bitmask of VkMemoryPropertyFlags
};

/// Opaque description of an image to create.
struct ImageDesc {
    VkFormat   format  = VK_FORMAT_UNDEFINED;
    VkExtent3D extent  = {};
    uint32_t   mipLevels = 1;
    uint32_t   arrayLayers = 1;
    uint32_t   usage     = 0;   // bitmask of VkImageUsageFlagBits
    uint32_t   memoryFlags = 0;
};

struct ShaderModuleDesc {
    const uint32_t* code   = nullptr;
    size_t          words  = 0;
};

/// Per-frame timing sample reported by the performance monitor.
struct FrameStats {
    uint64_t frameNumber = 0;
    double   frameTimeMs  = 0.0;
    double   fps          = 0.0;
    uint64_t gpuTimeNs    = 0;
};

/// DxvkDevice — owns VkDevice + graphics queue + loader dispatch.
class DxvkDevice {
public:
    DxvkDevice(std::shared_ptr<DxvkAdapter> adapter,
               VkDevice handle,
               VkQueue  queue,
               uint32_t queueFamily);
    ~DxvkDevice();

    DxvkDevice(const DxvkDevice&) = delete;
    DxvkDevice& operator=(const DxvkDevice&) = delete;

    static std::shared_ptr<DxvkDevice> create(const VulkanLoader& loader,
                                              std::shared_ptr<DxvkAdapter> adapter);

    VkDevice device() const noexcept { return m_device; }
    VkQueue  queue()  const noexcept { return m_queue; }
    uint32_t queueFamily() const noexcept { return m_queueFamily; }
    DxvkAdapter& adapter() const noexcept { return *m_adapter; }

    /// Factory helpers — implemented in src/vulkan/*.cpp. They return raw
    /// Vulkan handles; ownership stays with the calling D3D wrapper object.
    VkBuffer       createBuffer(const BufferDesc& desc);
    VkDeviceMemory allocateMemory(uint64_t size, uint32_t memoryTypeBits,
                                  uint32_t propertyFlags);
    VkImage        createImage(const ImageDesc& desc);
    VkImageView    createImageView(VkImage image, VkFormat format);
    VkShaderModule createShaderModule(const ShaderModuleDesc& desc);
    VkPipelineCache createPipelineCache();
    VkRenderPass   createRenderPass(VkFormat colorFormat, VkFormat depthFormat);
    VkDescriptorPool createDescriptorPool(uint32_t maxSets,
                                          uint32_t maxUniformBuffers,
                                          uint32_t maxSampledImages,
                                          uint32_t maxSamplers);
    VkDescriptorSet allocateDescriptorSet(VkDescriptorPool pool,
                                          VkDescriptorSetLayout layout);

    void destroyBuffer(VkBuffer b);
    void destroyImage(VkImage i);
    void destroyImageView(VkImageView v);
    void destroyShaderModule(VkShaderModule m);
    void destroyPipeline(VkPipeline p);
    void destroyPipelineCache(VkPipelineCache c);
    void destroyRenderPass(VkRenderPass r);
    void destroyDescriptorPool(VkDescriptorPool p);
    void freeMemory(VkDeviceMemory m);

    void* mapMemory(VkDeviceMemory mem, uint64_t offset, uint64_t size);
    void  unmapMemory(VkDeviceMemory mem);

    /// Wait for all pending queue work to finish.
    void waitIdle();

    /// Submit a command buffer to the graphics queue (one-shot semaphore /
    /// fence wiring). Stubs in the absence of a real ICD.
    void submitCommandBuffer(VkCommandBuffer cmd,
                             VkSemaphore waitSemaphore,
                             VkSemaphore signalSemaphore,
                             VkFence fence);

private:
    std::shared_ptr<DxvkAdapter> m_adapter;
    VkDevice                     m_device = nullptr;
    VkQueue                      m_queue  = nullptr;
    uint32_t                     m_queueFamily = 0;
};

} // namespace dxvk
