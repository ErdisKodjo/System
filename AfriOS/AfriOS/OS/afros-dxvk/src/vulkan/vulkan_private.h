// SPDX-License-Identifier: MIT
//
// vulkan_private.h — shared Vulkan CreateInfo structs + function-pointer
// typedefs used by every src/vulkan/*.cpp translation unit.
//
// The real DXVK build links against <vulkan/vulkan.h> and uses the official
// typed prototypes directly. AfriOS keeps the port self-contained (syntax-
// checkable without a Vulkan SDK) by re-declaring the minimal subset of
// CreateInfo structs and `PFN_vk*` typedefs the back-end needs. Layouts
// mirror the real Vulkan headers so handles exchanged with an ICD stay
// binary-compatible.

#pragma once

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"

#include <cstdint>

namespace dxvk::vk {

// VkStructureType (subset).
enum StructureType : int32_t {
    STRUCTURE_TYPE_BUFFER_CREATE_INFO          = 8,
    STRUCTURE_TYPE_IMAGE_CREATE_INFO           = 14,
    STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO        = 18,
    STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO   = 24,
    STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO      = 25,
    STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 26,
    STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO= 27,
    STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR   = 1000001000,
    STRUCTURE_TYPE_FENCE_CREATE_INFO           = 30,
    STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO       = 31,
    STRUCTURE_TYPE_SUBMIT_INFO                 = 32,
    STRUCTURE_TYPE_PRESENT_INFO_KHR            = 1000001001,
    STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO  = 34,
    STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO= 33,
};

// Minimal CreateInfo structs (only the fields the back-end actually sets).
struct BufferCreateInfo {
    int32_t  sType = STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    const void* pNext = nullptr;
    uint32_t flags = 0;
    uint64_t size = 0;
    uint32_t usage = 0;
    uint32_t sharingMode = 0;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;
};
struct ImageCreateInfo {
    int32_t  sType = STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    const void* pNext = nullptr;
    uint32_t flags = 0;
    uint32_t imageType = 0; // 2D
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent = {};
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    uint32_t samples = 1;
    uint32_t tiling = 0;
    uint32_t usage = 0;
    uint32_t initialLayout = 0;
    uint32_t sharingMode = 0;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;
};
struct MemoryAllocateInfo {
    int32_t  sType = STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    const void* pNext = nullptr;
    uint64_t allocationSize = 0;
    uint32_t memoryTypeIndex = 0;
};
struct ShaderModuleCreateInfo {
    int32_t  sType = STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    const void* pNext = nullptr;
    uint32_t flags = 0;
    size_t   codeSize = 0;
    const uint32_t* pCode = nullptr;
};
struct ImageViewCreateInfo {
    int32_t  sType = STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    const void* pNext = nullptr;
    VkImage  image = nullptr;
    uint32_t viewType = 0; // 2D
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t components = 0;
    uint32_t subresourceRange = 0;
};
struct DescriptorPoolCreateInfo {
    int32_t  sType = STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    const void* pNext = nullptr;
    uint32_t flags = 0;
    uint32_t maxSets = 0;
    uint32_t poolSizeCount = 0;
    const void* pPoolSizes = nullptr;
};
struct DescriptorSetAllocateInfo {
    int32_t  sType = STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    const void* pNext = nullptr;
    VkDescriptorPool descriptorPool = nullptr;
    uint32_t descriptorSetCount = 0;
    const VkDescriptorSetLayout* pSetLayouts = nullptr;
};
struct SwapchainCreateInfoKHR {
    int32_t  sType = STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    const void* pNext = nullptr;
    uint32_t flags = 0;
    VkSurfaceKHR surface = nullptr;
    uint32_t minImageCount = 2;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    uint32_t imageColorSpace = 0;
    VkExtent2D imageExtent = {};
    uint32_t imageArrayLayers = 1;
    uint32_t imageUsage = 0;
    uint32_t imageSharingMode = 0;
    uint32_t queueFamilyIndexCount = 0;
    const uint32_t* pQueueFamilyIndices = nullptr;
    uint32_t preTransform = 0;
    uint32_t compositeAlpha = 0;
    uint32_t presentMode = 0;
    int32_t  clipped = 1;
    VkSwapchainKHR oldSwapchain = nullptr;
};
struct SubmitInfo {
    int32_t  sType = STRUCTURE_TYPE_SUBMIT_INFO;
    const void* pNext = nullptr;
    uint32_t waitSemaphoreCount = 0;
    const VkSemaphore* pWaitSemaphores = nullptr;
    const uint32_t* pWaitDstStageMask = nullptr;
    uint32_t commandBufferCount = 0;
    const VkCommandBuffer* pCommandBuffers = nullptr;
    uint32_t signalSemaphoreCount = 0;
    const VkSemaphore* pSignalSemaphores = nullptr;
};
struct PresentInfoKHR {
    int32_t  sType = STRUCTURE_TYPE_PRESENT_INFO_KHR;
    const void* pNext = nullptr;
    uint32_t waitSemaphoreCount = 0;
    const VkSemaphore* pWaitSemaphores = nullptr;
    uint32_t swapchainCount = 0;
    const VkSwapchainKHR* pSwapchains = nullptr;
    const uint32_t* pImageIndices = nullptr;
    VkResult* pResults = nullptr;
};

// Function-pointer typedefs (resolved at runtime via VulkanLoader).
using PFN_vkCreateBuffer        = VkResult (*)(VkDevice, const BufferCreateInfo*, const void*, VkBuffer*);
using PFN_vkDestroyBuffer       = void (*)(VkDevice, VkBuffer, const void*);
using PFN_vkCreateImage         = VkResult (*)(VkDevice, const ImageCreateInfo*, const void*, VkImage*);
using PFN_vkDestroyImage        = void (*)(VkDevice, VkImage, const void*);
using PFN_vkAllocateMemory      = VkResult (*)(VkDevice, const MemoryAllocateInfo*, const void*, VkDeviceMemory*);
using PFN_vkFreeMemory          = void (*)(VkDevice, VkDeviceMemory, const void*);
using PFN_vkMapMemory           = VkResult (*)(VkDevice, VkDeviceMemory, uint64_t, uint64_t, uint32_t, void**);
using PFN_vkUnmapMemory         = void (*)(VkDevice, VkDeviceMemory);
using PFN_vkCreateShaderModule  = VkResult (*)(VkDevice, const ShaderModuleCreateInfo*, const void*, VkShaderModule*);
using PFN_vkDestroyShaderModule = void (*)(VkDevice, VkShaderModule, const void*);
using PFN_vkCreateImageView     = VkResult (*)(VkDevice, const ImageViewCreateInfo*, const void*, VkImageView*);
using PFN_vkDestroyImageView    = void (*)(VkDevice, VkImageView, const void*);
using PFN_vkCreateDescriptorPool= VkResult (*)(VkDevice, const DescriptorPoolCreateInfo*, const void*, VkDescriptorPool*);
using PFN_vkDestroyDescriptorPool=void (*)(VkDevice, VkDescriptorPool, const void*);
using PFN_vkAllocateDescriptorSets = VkResult (*)(VkDevice, const DescriptorSetAllocateInfo*, VkDescriptorSet*);
using PFN_vkQueueSubmit         = VkResult (*)(VkQueue, uint32_t, const SubmitInfo*, VkFence);
using PFN_vkQueueWaitIdle       = VkResult (*)(VkQueue);
using PFN_vkDeviceWaitIdle      = VkResult (*)(VkDevice);

} // namespace dxvk::vk
