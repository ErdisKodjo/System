// SPDX-License-Identifier: MIT
//
// d3d12_description_heap.cpp — ID3D12DescriptorHeap implementation.
//
// D3D12 descriptor heaps map onto Vulkan descriptor pools. CBV/SRV/UAV heaps
// create a `VK_DESCRIPTOR_POOL_TYPE_UNIFORM_BUFFER / STORAGE_BUFFER /
// STORAGE_IMAGE` pool; Sampler heaps create a `SAMPLER` pool; RTV/DSV heaps
// don't have a direct Vulkan analogue (render-pass attachment indices are
// used instead) so we store them in a CPU-side array and hand out indices.
//
// `Allocate()` hands out a contiguous slot index + a real `VkDescriptorSet`
// (for CBV/SRV/UAV + Sampler heaps); `Free()` returns the slot to a free list.

#include "vulkan_loader.h"
#include "dxvk_device.h"
#include "d3d12_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::d3d12 {

/// One allocated descriptor slot.
struct DescriptorHandle {
    uint32_t          index = UINT32_MAX; // CPU-side slot index
    VkDescriptorSet   set   = nullptr;    // GPU descriptor set (CBV/SRV/UAV/Sampler)
    uint64_t          cpu   = 0;          // CPU descriptor handle (opaque)
};

/// D3D12DescriptorHeapImpl — concrete ID3D12DescriptorHeap.
class D3D12DescriptorHeapImpl : public ID3D12DescriptorHeap {
public:
    D3D12DescriptorHeapImpl(std::shared_ptr<DxvkDevice> device,
                            const DescriptorHeapDesc& desc)
        : m_device(std::move(device)), m_desc(desc) {
        m_slots.resize(desc.numDescriptors);
        for (uint32_t i = 0; i < desc.numDescriptors; ++i)
            m_freeList.push_back(i);
        createPool();
    }
    ~D3D12DescriptorHeapImpl() override {
        if (m_pool) m_device->destroyDescriptorPool(m_pool);
    }

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

    VkDescriptorPool vkPool() const override { return m_pool; }
    uint32_t capacity() const override  { return m_desc.numDescriptors; }
    uint32_t allocated() const override { return m_desc.numDescriptors - static_cast<uint32_t>(m_freeList.size()); }

    /// Allocate one descriptor. Returns false when the heap is full.
    bool Allocate(DescriptorHandle& out) {
        if (m_freeList.empty()) return false;
        const uint32_t idx = m_freeList.back();
        m_freeList.pop_back();
        out.index = idx;
        out.cpu   = reinterpret_cast<uint64_t>(this) + idx;
        out.set   = allocateSet();
        return true;
    }
    /// Return a descriptor to the heap.
    void Free(const DescriptorHandle& h) {
        if (h.index >= m_desc.numDescriptors) return;
        m_freeList.push_back(h.index);
    }

    const DescriptorHeapDesc& desc() const noexcept { return m_desc; }

private:
    void createPool() {
        if (m_desc.type == DescriptorHeapType::Rtv ||
            m_desc.type == DescriptorHeapType::Dsv) {
            // CPU-only heap; no VkDescriptorPool needed.
            return;
        }
        const uint32_t maxUbo   = m_desc.type == DescriptorHeapType::CbvSrvUav ? m_desc.numDescriptors : 0;
        const uint32_t maxSrv   = maxUbo;
        const uint32_t maxSmp   = m_desc.type == DescriptorHeapType::Sampler   ? m_desc.numDescriptors : 0;
        m_pool = m_device->createDescriptorPool(m_desc.numDescriptors,
                                                maxUbo, maxSrv, maxSmp);
    }
    VkDescriptorSet allocateSet() {
        // Each slot maps to one descriptor set laid out for the heap's type.
        // Real impl caches a per-shader layout; the skeleton reuses layout=nullptr.
        return m_pool ? m_device->allocateDescriptorSet(m_pool, nullptr) : nullptr;
    }

    std::shared_ptr<DxvkDevice> m_device;
    DescriptorHeapDesc          m_desc{};
    VkDescriptorPool            m_pool = nullptr;
    std::vector<DescriptorHandle> m_slots;
    std::vector<uint32_t>       m_freeList;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk::d3d12

// --- Trampoline used by d3d12_device.app::CreateDescriptorHeap -------------
extern "C" dxvk::d3d12::ID3D12DescriptorHeap* d3d12_heap_create(
    void* devicePtr, const dxvk::d3d12::DescriptorHeapDesc& desc) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::d3d12::D3D12DescriptorHeapImpl(std::move(device), desc);
}
