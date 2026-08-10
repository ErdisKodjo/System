// SPDX-License-Identifier: MIT
//
// vulkan_loader.h — minimal Vulkan loader + handle/type declarations for
// afros-dxvk.
//
// Real DXVK links against <vulkan/vulkan.h> and libvulkan.so.1. To keep the
// AfriOS port self-contained and syntax-checkable without a Vulkan SDK
// installed, this header declares just enough of the Vulkan surface to let
// every translation unit compile cleanly: opaque handle types, a handful of
// enum/struct shims used as function argument types, the VkResult enum, and a
// `VulkanLoader` class that dlopens libvulkan.so.1 at runtime and exposes
// `vkGetInstanceProcAddr` so the rest of the stack can resolve real entry
// points dynamically.
//
// Every handle is a typedef'd pointer to an opaque struct, matching the
// binary layout of the real Vulkan headers, so handles exchanged with a real
// ICD stay compatible. COM-style return codes (HRESULT, S_OK, E_FAIL, ...) are
// also declared here because they are shared by every D3D/DXGI translation
// unit and this is the lowest-level header everyone includes.

#pragma once

#include <cstdint>
#include <cstdio>

namespace dxvk {

// ---------------------------------------------------------------------------
// COM-style return codes (subset of Win32 HRESULT).
// ---------------------------------------------------------------------------
using HRESULT = int32_t;
using BOOL    = int32_t;

constexpr HRESULT S_OK            = static_cast<HRESULT>(0);
constexpr HRESULT S_FALSE         = static_cast<HRESULT>(1);
constexpr HRESULT E_FAIL          = static_cast<HRESULT>(0x80004005u);
constexpr HRESULT E_INVALIDARG    = static_cast<HRESULT>(0x80070057u);
constexpr HRESULT E_OUTOFMEMORY   = static_cast<HRESULT>(0x8007000Eu);
constexpr HRESULT E_NOTIMPL       = static_cast<HRESULT>(0x80004001u);
constexpr HRESULT E_NOINTERFACE   = static_cast<HRESULT>(0x80004002u);
constexpr HRESULT E_POINTER       = static_cast<HRESULT>(0x80004003u);
constexpr HRESULT DXGI_ERROR_INVALID_CALL = static_cast<HRESULT>(0x887A0001u);

constexpr BOOL TRUE  = 1;
constexpr BOOL FALSE = 0;

// ---------------------------------------------------------------------------
// Opaque Vulkan handle types. Real Vulkan headers typedef these as pointers
// to forward-declared opaque structs; we mirror that layout.
// ---------------------------------------------------------------------------
struct VkInstance_T;          using VkInstance         = VkInstance_T*;
struct VkPhysicalDevice_T;    using VkPhysicalDevice   = VkPhysicalDevice_T*;
struct VkDevice_T;            using VkDevice           = VkDevice_T*;
struct VkQueue_T;             using VkQueue            = VkQueue_T*;
struct VkCommandBuffer_T;     using VkCommandBuffer    = VkCommandBuffer_T*;
struct VkBuffer_T;            using VkBuffer           = VkBuffer_T*;
struct VkImage_T;             using VkImage            = VkImage_T*;
struct VkImageView_T;         using VkImageView        = VkImageView_T*;
struct VkDeviceMemory_T;      using VkDeviceMemory     = VkDeviceMemory_T*;
struct VkPipeline_T;          using VkPipeline         = VkPipeline_T*;
struct VkPipelineCache_T;     using VkPipelineCache    = VkPipelineCache_T*;
struct VkRenderPass_T;        using VkRenderPass       = VkRenderPass_T*;
struct VkFramebuffer_T2;      using VkFramebuffer      = VkFramebuffer_T2*;
struct VkShaderModule_T;      using VkShaderModule     = VkShaderModule_T*;
struct VkDescriptorPool_T;    using VkDescriptorPool   = VkDescriptorPool_T*;
struct VkDescriptorSet_T;     using VkDescriptorSet    = VkDescriptorSet_T*;
struct VkDescriptorSetLayout_T; using VkDescriptorSetLayout = VkDescriptorSetLayout_T*;
struct VkPipelineLayout_T;    using VkPipelineLayout   = VkPipelineLayout_T*;
struct VkSampler_T;           using VkSampler          = VkSampler_T*;
struct VkFence_T;             using VkFence            = VkFence_T*;
struct VkSemaphore_T;         using VkSemaphore        = VkSemaphore_T*;
struct VkSwapchainKHR_T;      using VkSwapchainKHR     = VkSwapchainKHR_T*;
struct VkSurfaceKHR_T;        using VkSurfaceKHR       = VkSurfaceKHR_T*;
struct VkQueryPool_T;         using VkQueryPool        = VkQueryPool_T*;
struct VkCommandPool_T;       using VkCommandPool      = VkCommandPool_T*;
struct VkBufferView_T;        using VkBufferView       = VkBufferView_T*;

// Null-handle constants (Vulkan uses 0 / nullptr for VK_NULL_HANDLE).
constexpr VkInstance         VK_NULL_HANDLE_INSTANCE   = nullptr;

// ---------------------------------------------------------------------------
// VkResult (subset).
// ---------------------------------------------------------------------------
enum VkResult : int32_t {
    VK_SUCCESS                        = 0,
    VK_NOT_READY                      = 1,
    VK_TIMEOUT                        = 2,
    VK_EVENT_SET                      = 3,
    VK_EVENT_RESET                    = 4,
    VK_INCOMPLETE                     = 5,
    VK_ERROR_OUT_OF_HOST_MEMORY       = -1,
    VK_ERROR_OUT_OF_DEVICE_MEMORY     = -2,
    VK_ERROR_INITIALIZATION_FAILED    = -3,
    VK_ERROR_DEVICE_LOST              = -4,
    VK_ERROR_MEMORY_MAP_FAILED        = -5,
    VK_ERROR_LAYER_NOT_PRESENT        = -6,
    VK_ERROR_EXTENSION_NOT_PRESENT    = -7,
    VK_ERROR_FEATURE_NOT_PRESENT      = -8,
    VK_ERROR_INCOMPATIBLE_DRIVER      = -9,
    VK_ERROR_TOO_MANY_OBJECTS         = -10,
    VK_ERROR_FORMAT_NOT_SUPPORTED     = -11,
    VK_ERROR_FRAGMENTED_POOL          = -12,
    VK_SUBOPTIMAL_KHR                 = 1000001003,
    VK_ERROR_OUT_OF_DATE_KHR          = -1000001004,
};

// ---------------------------------------------------------------------------
// A few enum/struct shims referenced as argument types in wrapper methods.
// Names mirror the real Vulkan headers; values are illustrative only.
// ---------------------------------------------------------------------------
enum VkFormat : int32_t {
    VK_FORMAT_UNDEFINED               = 0,
    VK_FORMAT_R8G8B8A8_UNORM          = 37,
    VK_FORMAT_B8G8R8A8_UNORM          = 44,
    VK_FORMAT_R8G8B8A8_SRGB           = 43,
    VK_FORMAT_B8G8R8A8_SRGB           = 50,
    VK_FORMAT_D32_SFLOAT              = 126,
    VK_FORMAT_D24_UNORM_S8_UINT       = 129,
    VK_FORMAT_R16G16B16A16_SFLOAT     = 97,
};

enum VkImageLayout : int32_t {
    VK_IMAGE_LAYOUT_UNDEFINED                = 0,
    VK_IMAGE_LAYOUT_GENERAL                  = 1,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL = 2,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL     = 6,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL     = 7,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR          = 1000001002,
};

enum VkQueueFlagBits : int32_t {
    VK_QUEUE_GRAPHICS_BIT = 1,
    VK_QUEUE_COMPUTE_BIT  = 2,
    VK_QUEUE_TRANSFER_BIT = 4,
};

struct VkExtent2D { uint32_t width; uint32_t height; };
struct VkExtent3D { uint32_t width; uint32_t height; uint32_t depth; };
struct VkOffset2D { int32_t x; int32_t y; };

struct VkViewport {
    float x;        float y;
    float width;    float height;
    float minDepth; float maxDepth;
};

struct VkRect2D { VkOffset2D offset; VkExtent2D extent; };

// Function-pointer typedefs for the loader entry points we actually call.
using PFN_vkGetInstanceProcAddr = void* (*)(VkInstance, const char*);
using PFN_vkGetDeviceProcAddr   = void* (*)(VkDevice,   const char*);
using PFN_vkCreateInstance      = VkResult (*)(const void*, const void*, VkInstance*);
using PFN_vkDestroyInstance     = void (*)(VkInstance, const void*);

// ---------------------------------------------------------------------------
// VulkanLoader — runtime dlopen of libvulkan.so.1.
//
// Real DXVK links libvulkan directly. We mirror that lazily so the module can
// be syntax-checked and even loaded into a host without a Vulkan ICD; the
// translation layers just degrade to no-op stubs when the loader fails.
// ---------------------------------------------------------------------------
class VulkanLoader {
public:
    VulkanLoader();
    ~VulkanLoader();

    VulkanLoader(const VulkanLoader&) = delete;
    VulkanLoader& operator=(const VulkanLoader&) = delete;

    /// True when libvulkan.so.1 was successfully loaded.
    bool valid() const noexcept { return m_lib != nullptr; }

    /// Resolved vkGetInstanceProcAddr (nullptr if libvulkan not present).
    PFN_vkGetInstanceProcAddr getInstanceProcAddr() const noexcept {
        return m_getInstanceProcAddr;
    }

    /// Generic lookup wrapper; returns nullptr when libvulkan is absent.
    void* getProc(const char* name) const noexcept;

private:
    void*                    m_lib = nullptr;
    PFN_vkGetInstanceProcAddr m_getInstanceProcAddr = nullptr;
};

} // namespace dxvk
