// SPDX-License-Identifier: MIT
//
// d3d12_types.h — shared D3D12 desc / enum / interface declarations used by
// every d3d12 translation unit. Kept minimal so each .cpp stays self-contained
// while still letting the device construct resource / heap / command-list
// wrappers defined in sibling translation units.

#pragma once

#include "vulkan_loader.h"
#include "dxvk_device.h"

#include <cstdint>
#include <memory>

namespace dxvk::d3d12 {

// --- Enums -----------------------------------------------------------------
enum class ResourceDimension : uint8_t {
    Unknown = 0, Buffer = 1, Texture1D = 2, Texture2D = 3,
    Texture3D = 4,
};

enum class HeapType : uint32_t {
    Default  = 1, // DEVICE_LOCAL
    Upload   = 2, // HOST_VISIBLE | HOST_COHERENT (CPU→GPU)
    Readback = 3, // HOST_VISIBLE | HOST_COHERENT (GPU→CPU)
    Custom   = 4,
};

enum class ResourceState : uint32_t {
    Common               = 0,
    VertexAndConstantBuffer = 0x1,
    IndexBuffer          = 0x2,
    RenderTarget         = 0x4,
    UnorderedAccess      = 0x8,
    DepthWrite           = 0x10,
    DepthRead            = 0x20,
    NonPixelShaderResource = 0x40,
    PixelShaderResource  = 0x80,
    CopyDest             = 0x400,
    CopySource           = 0x800,
    Present              = 0x100000,
};

enum class DescriptorHeapType : uint32_t {
    CbvSrvUav = 0,
    Sampler   = 1,
    Rtv       = 2,
    Dsv       = 3,
};

enum class CommandListType : uint32_t {
    Direct = 0,
    Bundle = 1,
    Compute = 2,
    Copy    = 3,
};

// --- Descriptors -----------------------------------------------------------
struct ResourceDesc {
    ResourceDimension dimension = ResourceDimension::Texture2D;
    uint64_t width      = 0;
    uint32_t height     = 0;
    uint16_t depthOrArraySize = 1;
    uint16_t mipLevels  = 1;
    uint32_t format     = 0;
    uint32_t sampleCount= 1;
    uint32_t flags      = 0;
};

struct HeapProperties {
    HeapType type = HeapType::Default;
    uint32_t creationNodeMask = 1;
    uint32_t visibleNodeMask  = 1;
};

struct DescriptorHeapDesc {
    DescriptorHeapType type = DescriptorHeapType::CbvSrvUav;
    uint32_t numDescriptors = 0;
    uint32_t flags = 0;
    uint32_t nodeMask = 0;
};

// --- Minimal COM interfaces ------------------------------------------------
struct ID3D12Object {
    virtual ~ID3D12Object() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
};

struct ID3D12Resource : ID3D12Object {
    virtual HRESULT Map(uint32_t subresource, const void* readRange,
                        void** data) = 0;
    virtual void Unmap(uint32_t subresource, const void* writtenRange) = 0;
    virtual ResourceState GetState() const = 0;
    virtual VkImage  vkImage()  const = 0;
    virtual VkBuffer vkBuffer() const = 0;
};

struct ID3D12DescriptorHeap : ID3D12Object {
    virtual VkDescriptorPool vkPool() const = 0;
    virtual uint32_t capacity() const = 0;
    virtual uint32_t allocated() const = 0;
};

struct ID3D12CommandList : ID3D12Object {
    virtual CommandListType type() const = 0;
};

// --- Trampolines (defined in sibling .cpp, called from d3d12_device) -------
extern "C" ID3D12Resource*       d3d12_resource_create(void* devicePtr,
                                                      const ResourceDesc& desc,
                                                      const HeapProperties& heap);
extern "C" ID3D12DescriptorHeap* d3d12_heap_create(void* devicePtr,
                                                   const DescriptorHeapDesc& desc);

} // namespace dxvk::d3d12
