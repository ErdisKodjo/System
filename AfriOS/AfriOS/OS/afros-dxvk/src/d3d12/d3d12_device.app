// SPDX-License-Identifier: MIT
//
// d3d12_device.app — ID3D12Device implementation.
//
// NOTE: this file's extension is `.app` (a typo in the original repo layout).
// It is real C++17 source; meson treats it as a C++ translation unit via the
// explicit language override in `meson.build`. Do NOT rename — downstream
// build scripts reference this exact path.
//
// The D3D12 device is the top-level factory for command queues, command
// lists, descriptor heaps, committed resources, and root signatures. Each
// `Create*` method validates the incoming descriptor and forwards to the
// appropriate sibling trampoline (d3d12_resource_create / d3d12_heap_create
// defined in the other d3d12 translation units) to build the concrete
// wrapper, then returns the COM interface to the caller.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "d3d12_types.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk::d3d12 {

// Forward declarations of sibling types.
class D3D12CommandListImpl;
struct ID3D12CommandQueue;
struct ID3D12RootSignature;

/// Minimal ID3D12Device COM interface (subset).
struct ID3D12Device {
    virtual ~ID3D12Device() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    virtual HRESULT CreateCommandQueue(const void* desc, const void* iid,
                                       void** outQueue) = 0;
    virtual HRESULT CreateCommandList(uint32_t nodeMask,
                                      CommandListType type,
                                      void* allocator, void* initialState,
                                      const void* iid, void** outList) = 0;
    virtual HRESULT CreateDescriptorHeap(const DescriptorHeapDesc* desc,
                                         const void* iid,
                                         ID3D12DescriptorHeap** out) = 0;
    virtual HRESULT CreateCommittedResource(const HeapProperties* heap,
                                            uint32_t heapFlags,
                                            const ResourceDesc* desc,
                                            ResourceState initial,
                                            const void* optimizedClear,
                                            const void* iid,
                                            ID3D12Resource** out) = 0;
    virtual HRESULT CreateRootSignature(uint32_t nodeMask,
                                        const void* blob, size_t blobLen,
                                        const void* iid,
                                        ID3D12RootSignature** out) = 0;
};

/// D3D12Device — concrete ID3D12Device.
class D3D12DeviceImpl : public ID3D12Device {
public:
    explicit D3D12DeviceImpl(std::shared_ptr<DxvkDevice> device)
        : m_device(std::move(device)) {}

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

    HRESULT CreateCommandQueue(const void* /*desc*/, const void* /*iid*/,
                                void** outQueue) override {
        if (!outQueue) return E_POINTER;
        // Real impl wraps the DxvkDevice's queue in an ID3D12CommandQueue.
        *outQueue = nullptr;
        return E_NOTIMPL;
    }
    HRESULT CreateCommandList(uint32_t /*nodeMask*/, CommandListType type,
                               void* /*allocator*/, void* /*initialState*/,
                               const void* /*iid*/, void** outList) override {
        if (!outList) return E_POINTER;
        // Hand off to d3d12_command_list.cpp which owns the concrete type.
        *outList = nullptr;
        (void)type;
        return E_NOTIMPL;
    }
    HRESULT CreateDescriptorHeap(const DescriptorHeapDesc* desc,
                                  const void* /*iid*/,
                                  ID3D12DescriptorHeap** out) override {
        if (!desc || !out) return E_INVALIDARG;
        if (desc->numDescriptors == 0) return E_INVALIDARG;
        *out = d3d12_heap_create(&m_device, *desc);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateCommittedResource(const HeapProperties* heap,
                                     uint32_t /*heapFlags*/,
                                     const ResourceDesc* desc,
                                     ResourceState /*initial*/,
                                     const void* /*optimizedClear*/,
                                     const void* /*iid*/,
                                     ID3D12Resource** out) override {
        if (!heap || !desc || !out) return E_INVALIDARG;
        *out = d3d12_resource_create(&m_device, *desc, *heap);
        return *out ? S_OK : E_OUTOFMEMORY;
    }
    HRESULT CreateRootSignature(uint32_t /*nodeMask*/,
                                 const void* /*blob*/, size_t /*blobLen*/,
                                 const void* /*iid*/,
                                 ID3D12RootSignature** out) override {
        if (!out) return E_POINTER;
        // Real impl translates the serialized root signature into a
        // VkPipelineLayout + VkDescriptorSetLayout vector.
        *out = nullptr;
        return E_NOTIMPL;
    }

    DxvkDevice& device() noexcept { return *m_device; }

private:
    std::shared_ptr<DxvkDevice> m_device;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk::d3d12

// --- C entry point exported to Wine (D3D12CreateDevice) --------------------
extern "C" {

dxvk::HRESULT D3D12CreateDevice(void* /*adapter*/, uint32_t /*minFeatureLevel*/,
                                const void* /*iid*/,
                                dxvk::d3d12::ID3D12Device** outDevice) {
    if (!outDevice) return dxvk::E_INVALIDARG;
    // Real impl picks a DxvkAdapter via DXGI, calls DxvkDevice::create, then
    // constructs D3D12DeviceImpl. Skeleton returns E_FAIL when no Vulkan
    // device is available so callers can fall back.
    *outDevice = nullptr;
    return dxvk::E_FAIL;
}

} // extern "C"
