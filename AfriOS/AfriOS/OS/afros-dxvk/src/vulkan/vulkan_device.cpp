// SPDX-License-Identifier: MIT
//
// vulkan_device.cpp — DxvkDevice implementation.
//
// The DxvkDevice owns the logical VkDevice + its primary graphics queue and
// exposes high-level factory helpers (createBuffer / createImage /
// createShaderModule / ...) that the D3D translation layers call instead of
// touching Vulkan directly. Each helper resolves the matching `vk*` entry
// point through the VulkanLoader, builds the appropriate CreateInfo, and
// invokes the ICD. When libvulkan is absent, all factories degrade to
// returning sentinel handles so the rest of the stack can still link + run
// (and fail gracefully at the first real GPU operation).

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "vulkan_private.h"

#include <cstdint>
#include <cstdio>
#include <memory>

namespace dxvk {

namespace {

/// Pull the vk* PFN typedefs + CreateInfo structs into scope.
using namespace vk;

/// One-per-process loader (lazy-init).
const VulkanLoader& loader() {
    static VulkanLoader g_loader;
    return g_loader;
}

/// Resolve a vk entry point from the loader. Returns nullptr when libvulkan
/// is not present.
void* resolveProc(const char* name) {
    const auto& ldr = loader();
    if (!ldr.valid()) return nullptr;
    auto pfn = ldr.getInstanceProcAddr();
    return pfn ? pfn(nullptr, name) : nullptr;
}

/// Trait helper: resolve + cast a typed function pointer.
template <typename Fn>
Fn resolve(const char* name) {
    return reinterpret_cast<Fn>(resolveProc(name));
}

} // namespace

// --- DxvkDevice lifetime ---------------------------------------------------

DxvkDevice::DxvkDevice(std::shared_ptr<DxvkAdapter> adapter,
                       VkDevice handle, VkQueue queue, uint32_t queueFamily)
    : m_adapter(std::move(adapter))
    , m_device(handle)
    , m_queue(queue)
    , m_queueFamily(queueFamily) {}

DxvkDevice::~DxvkDevice() {
    // Real impl: vkDestroyDevice(m_device, nullptr). The loader owns the
    // VkInstance / VkPhysicalDevice; the device is the only thing we destroy
    // here. We intentionally do nothing in the skeleton because `m_device` is
    // typically a sentinel when libvulkan is absent.
    m_device = nullptr;
    m_queue  = nullptr;
}

std::shared_ptr<DxvkDevice> DxvkDevice::create(const VulkanLoader& /*ldr*/,
                                               std::shared_ptr<DxvkAdapter> adapter) {
    if (!adapter || !adapter->handle()) return nullptr;
    // Real impl: pick a queue family, fill VkDeviceQueueCreateInfo, enumerate
    // extensions (VK_KHR_swapchain etc.), call vkCreateDevice. The skeleton
    // returns nullptr so D3D factories degrade gracefully.
    return nullptr;
}

// --- Factory helpers -------------------------------------------------------

VkBuffer DxvkDevice::createBuffer(const BufferDesc& desc) {
    auto pfn = resolve<PFN_vkCreateBuffer>("vkCreateBuffer");
    if (!pfn || !m_device) return reinterpret_cast<VkBuffer>(static_cast<uintptr_t>(0x1));
    vk::BufferCreateInfo ci{};
    ci.size  = desc.size;
    ci.usage = desc.usage;
    VkBuffer out = nullptr;
    if (pfn(m_device, &ci, nullptr, &out) != VK_SUCCESS) return nullptr;
    return out;
}

VkDeviceMemory DxvkDevice::allocateMemory(uint64_t size, uint32_t /*typeBits*/,
                                          uint32_t /*propertyFlags*/) {
    auto pfn = resolve<PFN_vkAllocateMemory>("vkAllocateMemory");
    if (!pfn || !m_device) return reinterpret_cast<VkDeviceMemory>(static_cast<uintptr_t>(0x1));
    vk::MemoryAllocateInfo ai{};
    ai.allocationSize = size;
    ai.memoryTypeIndex = 0; // real impl picks via memoryTypes + propertyFlags
    VkDeviceMemory out = nullptr;
    if (pfn(m_device, &ai, nullptr, &out) != VK_SUCCESS) return nullptr;
    return out;
}

VkImage DxvkDevice::createImage(const ImageDesc& desc) {
    auto pfn = resolve<PFN_vkCreateImage>("vkCreateImage");
    if (!pfn || !m_device) return reinterpret_cast<VkImage>(static_cast<uintptr_t>(0x1));
    vk::ImageCreateInfo ci{};
    ci.format     = desc.format;
    ci.extent     = desc.extent;
    ci.mipLevels  = desc.mipLevels;
    ci.arrayLayers= desc.arrayLayers;
    ci.usage      = desc.usage;
    VkImage out = nullptr;
    if (pfn(m_device, &ci, nullptr, &out) != VK_SUCCESS) return nullptr;
    return out;
}

VkImageView DxvkDevice::createImageView(VkImage image, VkFormat format) {
    auto pfn = resolve<PFN_vkCreateImageView>("vkCreateImageView");
    if (!pfn || !m_device) return reinterpret_cast<VkImageView>(static_cast<uintptr_t>(0x1));
    vk::ImageViewCreateInfo ci{};
    ci.image  = image;
    ci.format = format;
    VkImageView out = nullptr;
    if (pfn(m_device, &ci, nullptr, &out) != VK_SUCCESS) return nullptr;
    return out;
}

VkShaderModule DxvkDevice::createShaderModule(const ShaderModuleDesc& desc) {
    auto pfn = resolve<PFN_vkCreateShaderModule>("vkCreateShaderModule");
    if (!pfn || !m_device) return reinterpret_cast<VkShaderModule>(static_cast<uintptr_t>(0x1));
    vk::ShaderModuleCreateInfo ci{};
    ci.codeSize = desc.words * sizeof(uint32_t);
    ci.pCode    = desc.code;
    VkShaderModule out = nullptr;
    if (pfn(m_device, &ci, nullptr, &out) != VK_SUCCESS) return nullptr;
    return out;
}

VkPipelineCache DxvkDevice::createPipelineCache() {
    // Real impl: vkCreatePipelineCache with initial data loaded from
    // /var/cache/afros-dxvk/pipeline.bin (see util/cache_manager.cpp).
    return reinterpret_cast<VkPipelineCache>(static_cast<uintptr_t>(0x1));
}

VkRenderPass DxvkDevice::createRenderPass(VkFormat /*colorFormat*/,
                                          VkFormat /*depthFormat*/) {
    return reinterpret_cast<VkRenderPass>(static_cast<uintptr_t>(0x1));
}

VkDescriptorPool DxvkDevice::createDescriptorPool(uint32_t maxSets,
                                                  uint32_t /*maxUbo*/,
                                                  uint32_t /*maxSrv*/,
                                                  uint32_t /*maxSmp*/) {
    auto pfn = resolve<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool");
    if (!pfn || !m_device) return reinterpret_cast<VkDescriptorPool>(static_cast<uintptr_t>(0x1));
    vk::DescriptorPoolCreateInfo ci{};
    ci.maxSets = maxSets;
    VkDescriptorPool out = nullptr;
    if (pfn(m_device, &ci, nullptr, &out) != VK_SUCCESS) return nullptr;
    return out;
}

VkDescriptorSet DxvkDevice::allocateDescriptorSet(VkDescriptorPool pool,
                                                  VkDescriptorSetLayout /*layout*/) {
    auto pfn = resolve<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets");
    if (!pfn || !m_device) return reinterpret_cast<VkDescriptorSet>(static_cast<uintptr_t>(0x1));
    vk::DescriptorSetAllocateInfo ai{};
    ai.descriptorPool = pool;
    ai.descriptorSetCount = 1;
    VkDescriptorSet out = nullptr;
    if (pfn(m_device, &ai, &out) != VK_SUCCESS) return nullptr;
    return out;
}

// --- Destroy helpers -------------------------------------------------------

void DxvkDevice::destroyBuffer(VkBuffer b) {
    if (auto pfn = resolve<PFN_vkDestroyBuffer>("vkDestroyBuffer")) pfn(m_device, b, nullptr);
}
void DxvkDevice::destroyImage(VkImage i) {
    if (auto pfn = resolve<PFN_vkDestroyImage>("vkDestroyImage")) pfn(m_device, i, nullptr);
}
void DxvkDevice::destroyImageView(VkImageView v) {
    if (auto pfn = resolve<PFN_vkDestroyImageView>("vkDestroyImageView")) pfn(m_device, v, nullptr);
}
void DxvkDevice::destroyShaderModule(VkShaderModule m) {
    if (auto pfn = resolve<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")) pfn(m_device, m, nullptr);
}
void DxvkDevice::destroyPipeline(VkPipeline /*p*/) {
    // vkDestroyPipeline resolved lazily; omitted for brevity.
}
void DxvkDevice::destroyPipelineCache(VkPipelineCache /*c*/) {}
void DxvkDevice::destroyRenderPass(VkRenderPass /*r*/) {}
void DxvkDevice::destroyDescriptorPool(VkDescriptorPool p) {
    if (auto pfn = resolve<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")) pfn(m_device, p, nullptr);
}
void DxvkDevice::freeMemory(VkDeviceMemory m) {
    if (auto pfn = resolve<PFN_vkFreeMemory>("vkFreeMemory")) pfn(m_device, m, nullptr);
}

void* DxvkDevice::mapMemory(VkDeviceMemory mem, uint64_t offset, uint64_t size) {
    auto pfn = resolve<PFN_vkMapMemory>("vkMapMemory");
    if (!pfn || !m_device) return nullptr;
    void* ptr = nullptr;
    if (pfn(m_device, mem, offset, size, 0, &ptr) != VK_SUCCESS) return nullptr;
    return ptr;
}
void DxvkDevice::unmapMemory(VkDeviceMemory mem) {
    if (auto pfn = resolve<PFN_vkUnmapMemory>("vkUnmapMemory")) pfn(m_device, mem);
}

void DxvkDevice::waitIdle() {
    if (auto pfn = resolve<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle")) pfn(m_device);
}

void DxvkDevice::submitCommandBuffer(VkCommandBuffer cmd,
                                     VkSemaphore waitSemaphore,
                                     VkSemaphore signalSemaphore,
                                     VkFence fence) {
    auto pfn = resolve<PFN_vkQueueSubmit>("vkQueueSubmit");
    if (!pfn || !m_queue) { (void)cmd; return; }
    vk::SubmitInfo si{};
    if (waitSemaphore)   { si.waitSemaphoreCount = 1; si.pWaitSemaphores = &waitSemaphore; }
    if (cmd)             { si.commandBufferCount = 1; si.pCommandBuffers  = &cmd; }
    if (signalSemaphore) { si.signalSemaphoreCount = 1; si.pSignalSemaphores = &signalSemaphore; }
    pfn(m_queue, 1, &si, fence);
}

} // namespace dxvk
