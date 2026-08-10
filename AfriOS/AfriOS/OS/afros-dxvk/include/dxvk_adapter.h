// SPDX-License-Identifier: MIT
//
// dxvk_adapter.h — DxvkAdapter, the AfriOS wrapper around a VkPhysicalDevice.
//
// The adapter is the bridge between DXGI's `IDXGIAdapter` (exposed to Wine
// callers) and the underlying Vulkan physical device. It owns the queried
// properties/features of a single `VkPhysicalDevice`, enumerates its queue
// families, exposes memory type information used by the memory allocator, and
// answers format-support queries the translation layers need when validating
// D3D9/D3D11/D3D12 surface formats.

#pragma once

#include "vulkan_loader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dxvk {

/// Subset of VkPhysicalDeviceProperties that the translation layers consult.
struct AdapterProperties {
    uint32_t    apiVersion    = 0;
    uint32_t    driverVersion = 0;
    uint32_t    vendorID      = 0;
    uint32_t    deviceID      = 0;
    char        deviceName[256] = {};
    uint32_t    maxImageDimension2D = 0;
};

/// One queue family descriptor.
struct AdapterQueueFamily {
    uint32_t index       = 0;
    uint32_t queueCount  = 0;
    uint32_t queueFlags  = 0;   // bitmask of VkQueueFlagBits
    bool     supportsPresent = false;
};

/// One memory type from VkPhysicalDeviceMemoryProperties.
struct AdapterMemoryType {
    uint32_t propertyFlags = 0;
    uint32_t heapIndex     = 0;
};

/// One memory heap.
struct AdapterMemoryHeap {
    uint64_t size  = 0;
    uint32_t flags = 0;
};

struct AdapterMemoryInfo {
    std::vector<AdapterMemoryType> types;
    std::vector<AdapterMemoryHeap> heaps;
};

/// Format-support query result.
enum class FormatSupport : uint32_t {
    None        = 0,
    Sampled     = 1u << 0,
    Storage     = 1u << 1,
    Renderable  = 1u << 2,
    DepthStencil= 1u << 3,
    BlitSrc     = 1u << 4,
    BlitDst     = 1u << 5,
};

inline FormatSupport operator|(FormatSupport a, FormatSupport b) {
    return static_cast<FormatSupport>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline uint32_t operator&(FormatSupport a, FormatSupport b) {
    return static_cast<uint32_t>(a) & static_cast<uint32_t>(b);
}

/// DxvkAdapter — owns one VkPhysicalDevice and its queried metadata.
class DxvkAdapter {
public:
    explicit DxvkAdapter(VkPhysicalDevice handle);
    ~DxvkAdapter();

    DxvkAdapter(const DxvkAdapter&) = delete;
    DxvkAdapter& operator=(const DxvkAdapter&) = delete;

    /// Raw Vulkan handle (may be nullptr when no ICD is present).
    VkPhysicalDevice handle() const noexcept { return m_handle; }

    const AdapterProperties& properties() const noexcept { return m_props; }
    const std::vector<AdapterQueueFamily>& queueFamilies() const noexcept {
        return m_queueFamilies;
    }
    const AdapterMemoryInfo& memory() const noexcept { return m_memory; }

    /// Selects the best queue family satisfying the requested flags and
    /// present support. Returns UINT32_MAX when none qualifies.
    uint32_t findQueueFamily(uint32_t requiredFlags, bool requirePresent) const noexcept;

    /// Queries whether a format is usable for the requested usage flags.
    FormatSupport queryFormatSupport(VkFormat format) const;

    /// Pretty device name (for DXGI adapter description + logs).
    const char* name() const noexcept { return m_props.deviceName; }

    /// Vendor / device identifiers (used by DXGI GetDesc).
    uint32_t vendorId() const noexcept { return m_props.vendorID; }
    uint32_t deviceId() const noexcept { return m_props.deviceID; }

    /// Lazily populates m_props / m_queueFamilies / m_memory by querying the
    /// ICD through the supplied loader. Safe to call repeatedly.
    void refresh(const VulkanLoader& loader);

private:
    VkPhysicalDevice           m_handle = nullptr;
    AdapterProperties          m_props{};
    std::vector<AdapterQueueFamily> m_queueFamilies;
    AdapterMemoryInfo          m_memory;
};

} // namespace dxvk
