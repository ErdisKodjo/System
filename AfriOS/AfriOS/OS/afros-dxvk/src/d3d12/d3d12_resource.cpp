// SPDX-License-Identifier: MIT
//
// d3d12_resource.cpp — ID3D12Resource implementation wrapping a VkImage or
// VkBuffer with backing VkDeviceMemory and resource-state tracking.
//
// D3D12 resources are "committed" — each carries its own heap allocation —
// and track a `ResourceState` so that `ResourceBarrier()` on a command list
// can emit the right VkImageMemoryBarrier / VkBufferMemoryBarrier. `Map()` /
// `Unmap()` route through `vkMapMemory` for UPLOAD/READBACK heaps; default
// heaps require a staging buffer (handled by the command-list `CopyResource`
// path).

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "d3d12_types.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk::d3d12 {

/// D3D12ResourceImpl — concrete ID3D12Resource.
class D3D12ResourceImpl : public ID3D12Resource {
public:
    D3D12ResourceImpl(std::shared_ptr<DxvkDevice> device,
                      const ResourceDesc& desc,
                      const HeapProperties& heap)
        : m_device(std::move(device)), m_desc(desc), m_heap(heap) {
        allocate();
    }
    ~D3D12ResourceImpl() override { deallocate(); }

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

    HRESULT Map(uint32_t subresource, const void* /*readRange*/,
                void** data) override {
        if (!data) return E_POINTER;
        if (m_mapped) return E_INVALIDARG;
        if (m_heap.type != HeapType::Upload &&
            m_heap.type != HeapType::Readback) {
            // Default heap: caller must use a staging buffer via CopyResource.
            *data = nullptr;
            return E_INVALIDARG;
        }
        m_mapped = static_cast<char*>(
            m_device->mapMemory(m_memory, 0, allocationSize()));
        if (!m_mapped) return E_FAIL;
        m_mappedSub = subresource;
        *data = m_mapped;
        return S_OK;
    }
    void Unmap(uint32_t subresource, const void* /*writtenRange*/) override {
        if (subresource != m_mappedSub) return;
        if (m_mapped) m_device->unmapMemory(m_memory);
        m_mapped = nullptr;
        m_mappedSub = UINT32_MAX;
    }

    ResourceState GetState() const override { return m_state; }
    void SetState(ResourceState s) noexcept { m_state = s; }

    VkImage  vkImage()  const override { return m_image; }
    VkBuffer vkBuffer() const override { return m_buffer; }

    const ResourceDesc& desc() const noexcept { return m_desc; }
    uint64_t allocationSize() const noexcept {
        if (m_desc.dimension == ResourceDimension::Buffer) return m_desc.width;
        // Texel size * width * height * array * mips (rough).
        const uint32_t bpp = 4;
        uint64_t bytes = 0;
        uint32_t w = static_cast<uint32_t>(m_desc.width);
        uint32_t h = m_desc.height;
        for (uint16_t m = 0; m < m_desc.mipLevels; ++m) {
            bytes += static_cast<uint64_t>(w) * h * bpp;
            if (w > 1) w >>= 1;
            if (h > 1) h >>= 1;
        }
        return bytes * m_desc.depthOrArraySize;
    }

private:
    void allocate() {
        const uint32_t memFlags = translateMemFlags(m_heap.type);
        if (m_desc.dimension == ResourceDimension::Buffer) {
            dxvk::BufferDesc b{};
            b.size = m_desc.width;
            b.usage = 0x00000040 /*UNIFORM*/ | 0x00000020 /*STORAGE*/
                    | 0x00000008 /*VERTEX*/ | 0x00000010 /*INDEX*/
                    | 0x00000002 /*TRANSFER_SRC*/ | 0x00000001 /*TRANSFER_DST*/;
            b.memoryFlags = memFlags;
            m_buffer = m_device->createBuffer(b);
            m_memory = m_device->allocateMemory(b.size, ~0u, memFlags);
            m_state = ResourceState::Common;
        } else {
            dxvk::ImageDesc i{};
            i.format = VK_FORMAT_B8G8R8A8_UNORM;
            i.extent = { static_cast<uint32_t>(m_desc.width),
                         m_desc.height, 1 };
            i.mipLevels   = m_desc.mipLevels;
            i.arrayLayers = m_desc.depthOrArraySize;
            i.usage       = 0x10 | 0x20 | 0x40 | 0x02 | 0x01;
            i.memoryFlags = memFlags;
            m_image  = m_device->createImage(i);
            m_memory = m_device->allocateMemory(allocationSize(), ~0u, memFlags);
            m_state  = m_desc.flags & 0x2 /*ALLOW_RENDER_TARGET*/
                       ? ResourceState::RenderTarget : ResourceState::Common;
        }
    }
    void deallocate() {
        if (m_image)  m_device->destroyImage(m_image);
        if (m_buffer) m_device->destroyBuffer(m_buffer);
        if (m_memory) m_device->freeMemory(m_memory);
        m_image = nullptr; m_buffer = nullptr; m_memory = nullptr;
    }
    static uint32_t translateMemFlags(HeapType t) {
        switch (t) {
            case HeapType::Upload:
            case HeapType::Readback:
                return 0x00000002u /*HOST_VISIBLE*/ | 0x00000004u /*HOST_COHERENT*/;
            default:
                return 0x00000000u /*DEVICE_LOCAL*/;
        }
    }

    std::shared_ptr<DxvkDevice> m_device;
    ResourceDesc                m_desc{};
    HeapProperties              m_heap{};
    VkImage                     m_image  = nullptr;
    VkBuffer                    m_buffer = nullptr;
    VkDeviceMemory              m_memory = nullptr;
    char*                       m_mapped = nullptr;
    uint32_t                    m_mappedSub = UINT32_MAX;
    ResourceState               m_state = ResourceState::Common;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk::d3d12

// --- Trampoline used by d3d12_device.app::CreateCommittedResource ---------
extern "C" dxvk::d3d12::ID3D12Resource* d3d12_resource_create(
    void* devicePtr, const dxvk::d3d12::ResourceDesc& desc,
    const dxvk::d3d12::HeapProperties& heap) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::d3d12::D3D12ResourceImpl(std::move(device), desc, heap);
}
