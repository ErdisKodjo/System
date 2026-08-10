// SPDX-License-Identifier: MIT
//
// d3d11_buffer.cpp — ID3D11Buffer implementation wrapping a Vulkan VkBuffer +
// its backing VkDeviceMemory.
//
// The buffer holds a `VkBuffer` handle, the bound `VkDeviceMemory`, its mapped
// host pointer (when `Map()` is active), and the D3D11 buffer description.
// `Map()`/`Unmap()` route to `vkMapMemory`/`vkUnmapMemory` on the parent
// DxvkDevice; staging buffers (USAGE_DYNAMIC) are allocated out of HOST_VISIBLE
// memory, default buffers (USAGE_DEFAULT/IMMUTABLE) out of DEVICE_LOCAL memory.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "d3d11_types.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk::d3d11 {

/// D3D11BufferImpl — concrete ID3D11Buffer.
class D3D11BufferImpl : public ID3D11Buffer {
public:
    D3D11BufferImpl(std::shared_ptr<DxvkDevice> device, const BufferDesc& desc)
        : m_device(std::move(device)), m_desc(desc) {
        allocate();
    }
    ~D3D11BufferImpl() override { deallocate(); }

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
    void GetDesc(BufferDesc* out) override { if (out) *out = m_desc; }

    HRESULT Map(MapType mapType, uint32_t /*mapFlags*/, void** data) override {
        if (!data) return E_POINTER;
        if (m_mapped) return E_INVALIDARG; // already mapped
        // Dynamic / staging buffers map persistently; default buffers would
        // route through a staging copy in a real implementation.
        if (m_desc.usage == Usage::Dynamic || m_desc.usage == Usage::Staging) {
            m_mapped = static_cast<char*>(
                m_device->mapMemory(m_memory, 0, m_desc.byteWidth));
            if (!m_mapped) return E_FAIL;
            if (mapType == MapType::WriteDiscard) {
                std::memset(m_mapped, 0, m_desc.byteWidth);
            }
            *data = m_mapped;
            return S_OK;
        }
        *data = nullptr;
        return E_INVALIDARG;
    }
    void Unmap() override {
        if (!m_mapped) return;
        m_device->unmapMemory(m_memory);
        m_mapped = nullptr;
    }

    VkBuffer      vkBuffer()  const noexcept { return m_buffer; }
    VkDeviceMemory vkMemory() const noexcept { return m_memory; }

private:
    void allocate() {
        dxvk::BufferDesc vk{};
        vk.size        = m_desc.byteWidth;
        vk.usage       = translateUsage(m_desc);
        vk.memoryFlags = translateMemFlags(m_desc);
        m_buffer = m_device->createBuffer(vk);
        m_memory = m_device->allocateMemory(vk.size, /*typeBits*/ ~0u,
                                            vk.memoryFlags);
    }
    void deallocate() {
        if (m_buffer) m_device->destroyBuffer(m_buffer);
        if (m_memory) m_device->freeMemory(m_memory);
        m_buffer = nullptr;
        m_memory = nullptr;
    }

    static uint32_t translateUsage(const BufferDesc& d) {
        uint32_t u = 0;
        if (d.bindFlags & BindVertexBuffer)    u |= 0x00000008; // VERTEX_BUFFER
        if (d.bindFlags & BindIndexBuffer)     u |= 0x00000010; // INDEX_BUFFER
        if (d.bindFlags & BindConstantBuffer)  u |= 0x00000040; // UNIFORM_BUFFER
        if (d.bindFlags & BindShaderResource)  u |= 0x00000020; // STORAGE_BUFFER
        if (d.bindFlags & BindUnorderedAccess) u |= 0x00000020;
        if (d.usage == Usage::Staging)         u |= 0x00000002; // TRANSFER_SRC
        return u;
    }
    static uint32_t translateMemFlags(const BufferDesc& d) {
        if (d.usage == Usage::Staging || d.usage == Usage::Dynamic)
            return 0x00000002u /* HOST_VISIBLE */ | 0x00000004u /* HOST_COHERENT */;
        return 0x00000000u /* DEVICE_LOCAL */;
    }

    std::shared_ptr<DxvkDevice> m_device;
    BufferDesc                  m_desc{};
    VkBuffer                    m_buffer = nullptr;
    VkDeviceMemory              m_memory = nullptr;
    char*                       m_mapped = nullptr;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk::d3d11

// --- Trampoline used by d3d11_device.cpp::CreateBuffer ---------------------
extern "C" dxvk::d3d11::ID3D11Buffer* d3d11_buffer_create(
    void* devicePtr, const dxvk::d3d11::BufferDesc& desc) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    auto* impl = new dxvk::d3d11::D3D11BufferImpl(std::move(device), desc);
    return impl;
}
