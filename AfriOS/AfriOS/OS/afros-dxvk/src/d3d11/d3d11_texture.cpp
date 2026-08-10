// SPDX-License-Identifier: MIT
//
// d3d11_texture.cpp — ID3D11Texture2D implementation wrapping a Vulkan VkImage
// + its backing VkDeviceMemory and per-mip VkImageViews.
//
// The texture owns one `VkImage`, one `VkDeviceMemory` allocation, and a
// vector of `VkImageView`s (one per mip / array slice that the app requested a
// view for). `Map()`/`Unmap()` operate per-subresource: in a real impl the
// device copies the requested subresource into a staging buffer (for read) or
// from a staging buffer back into the image (for write), since DEVICE_LOCAL
// images cannot be mapped directly.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "d3d11_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::d3d11 {

/// D3D11Texture2DImpl — concrete ID3D11Texture2D.
class D3D11Texture2DImpl : public ID3D11Texture2D {
public:
    D3D11Texture2DImpl(std::shared_ptr<DxvkDevice> device,
                       const Texture2DDesc& desc)
        : m_device(std::move(device)), m_desc(desc) {
        allocate();
    }
    ~D3D11Texture2DImpl() override { deallocate(); }

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
    void GetDesc(Texture2DDesc* out) override { if (out) *out = m_desc; }

    HRESULT Map(uint32_t subresource, MapType mapType, uint32_t /*flags*/,
                void** data) override {
        if (!data) return E_POINTER;
        if (subresource >= totalSubresources()) return E_INVALIDARG;
        if (m_mappedSub != UINT32_MAX) return E_INVALIDARG;
        // Staging textures are HOST_VISIBLE and can be mapped directly. Default
        // textures route through a staging buffer in a real implementation;
        // for the skeleton we surface a throwaway allocation sized to one mip.
        const uint64_t rowBytes   = static_cast<uint64_t>(m_desc.width) * bytesPerPixel();
        const uint64_t sliceBytes = rowBytes * m_desc.height;
        if (m_desc.usage == Usage::Staging) {
            m_mapped = static_cast<char*>(
                m_device->mapMemory(m_memory, 0, sliceBytes));
        } else {
            m_mapped = static_cast<char*>(::operator new(sliceBytes));
        }
        if (!m_mapped) return E_FAIL;
        if (mapType == MapType::WriteDiscard)
            std::memset(m_mapped, 0, static_cast<size_t>(sliceBytes));
        m_mappedSub = subresource;
        *data = m_mapped;
        return S_OK;
    }
    void Unmap(uint32_t subresource) override {
        if (subresource != m_mappedSub) return;
        if (m_desc.usage == Usage::Staging) {
            m_device->unmapMemory(m_memory);
        } else if (m_mapped) {
            ::operator delete(m_mapped);
        }
        m_mapped    = nullptr;
        m_mappedSub = UINT32_MAX;
    }

    VkImage       vkImage()  const noexcept { return m_image; }
    VkDeviceMemory vkMemory() const noexcept { return m_memory; }
    VkImageView   view(uint32_t index) const noexcept {
        return index < m_views.size() ? m_views[index] : nullptr;
    }

private:
    void allocate() {
        dxvk::ImageDesc vk{};
        vk.format      = mapFormat(m_desc.format);
        vk.extent      = { m_desc.width, m_desc.height, 1 };
        vk.mipLevels   = m_desc.mipLevels;
        vk.arrayLayers = m_desc.arraySize;
        vk.usage       = translateUsage(m_desc);
        vk.memoryFlags = translateMemFlags(m_desc);
        m_image  = m_device->createImage(vk);
        m_memory = m_device->allocateMemory(imageSize(), /*typeBits*/ ~0u,
                                            vk.memoryFlags);
        // Pre-create one view per mip for the most common SRV/RTV case.
        m_views.reserve(m_desc.mipLevels);
        for (uint16_t m = 0; m < m_desc.mipLevels; ++m) {
            m_views.push_back(m_device->createImageView(m_image, vk.format));
        }
    }
    void deallocate() {
        for (auto v : m_views) if (v) m_device->destroyImageView(v);
        m_views.clear();
        if (m_image)  m_device->destroyImage(m_image);
        if (m_memory) m_device->freeMemory(m_memory);
        m_image = nullptr; m_memory = nullptr;
    }

    uint32_t totalSubresources() const noexcept {
        return static_cast<uint32_t>(m_desc.mipLevels) * m_desc.arraySize;
    }
    uint64_t imageSize() const noexcept {
        uint64_t bytes = 0;
        uint32_t w = m_desc.width, h = m_desc.height;
        for (uint16_t m = 0; m < m_desc.mipLevels; ++m) {
            bytes += static_cast<uint64_t>(w) * h * bytesPerPixel();
            if (w > 1) w >>= 1;
            if (h > 1) h >>= 1;
        }
        return bytes * m_desc.arraySize;
    }
    uint32_t bytesPerPixel() const noexcept {
        // Map a handful of DXGI formats; default to 4.
        switch (m_desc.format) {
            case 28 /*DXGI_FORMAT_R8G8B8A8_UNORM*/:
            case 87 /*DXGI_FORMAT_B8G8R8A8_UNORM*/:
            case 29 /*DXGI_FORMAT_R8G8B8A8_UINT*/:  return 4;
            case 10 /*DXGI_FORMAT_R16G16B16A16_FLOAT*/: return 8;
            case 45 /*DXGI_FORMAT_D32_FLOAT*/:          return 4;
            case 55 /*DXGI_FORMAT_D24_UNORM_S8_UINT*/:  return 4;
            default: return 4;
        }
    }
    static VkFormat mapFormat(uint32_t dxgiFormat) {
        switch (dxgiFormat) {
            case 28: return VK_FORMAT_R8G8B8A8_UNORM;
            case 87: return VK_FORMAT_B8G8R8A8_UNORM;
            case 10: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case 45: return VK_FORMAT_D32_SFLOAT;
            case 55: return VK_FORMAT_D24_UNORM_S8_UINT;
            default: return VK_FORMAT_UNDEFINED;
        }
    }
    static uint32_t translateUsage(const Texture2DDesc& d) {
        uint32_t u = 0;
        if (d.bindFlags & BindShaderResource) u |= 0x00000010; // SAMPLED
        if (d.bindFlags & BindRenderTarget)   u |= 0x00000020; // COLOR_ATTACHMENT
        if (d.bindFlags & BindDepthStencil)   u |= 0x00000040; // DEPTH_STENCIL_ATTACHMENT
        if (d.bindFlags & BindUnorderedAccess)u |= 0x00000080; // STORAGE
        if (d.usage == Usage::Staging)        u |= 0x00000002 | 0x00000001; // TRANSFER
        else                                  u |= 0x00000002; // TRANSFER_DST
        return u;
    }
    static uint32_t translateMemFlags(const Texture2DDesc& d) {
        if (d.usage == Usage::Staging)
            return 0x00000002u | 0x00000004u; // HOST_VISIBLE | HOST_COHERENT
        return 0x00000000u; // DEVICE_LOCAL
    }

    std::shared_ptr<DxvkDevice>   m_device;
    Texture2DDesc                 m_desc{};
    VkImage                       m_image  = nullptr;
    VkDeviceMemory                m_memory = nullptr;
    std::vector<VkImageView>      m_views;
    char*                         m_mapped = nullptr;
    uint32_t                      m_mappedSub = UINT32_MAX;
    uint32_t                      m_refCount  = 1;
};

} // namespace dxvk::d3d11

// --- Trampoline used by d3d11_device.cpp::CreateTexture2D ------------------
extern "C" dxvk::d3d11::ID3D11Texture2D* d3d11_texture_create(
    void* devicePtr, const dxvk::d3d11::Texture2DDesc& desc) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::d3d11::D3D11Texture2DImpl(std::move(device), desc);
}
