// SPDX-License-Identifier: MIT
//
// d3d11_types.h — shared D3D11 desc / enum / interface declarations used by
// every d3d11 translation unit. Kept minimal so each .cpp stays self-contained
// while still letting the device construct buffer/texture/shader wrappers
// defined in sibling translation units.

#pragma once

#include "vulkan_loader.h"
#include "dxvk_device.h"

#include <cstdint>
#include <memory>

namespace dxvk::d3d11 {

// --- Enums -----------------------------------------------------------------
enum class Usage : uint32_t {
    Default   = 0,
    Immutable = 1,
    Dynamic   = 2,
    Staging   = 3,
};

enum BindFlag : uint32_t {
    BindNone          = 0,
    BindVertexBuffer  = 1u << 0,
    BindIndexBuffer   = 1u << 1,
    BindConstantBuffer= 1u << 2,
    BindShaderResource= 1u << 3,
    BindStreamOutput  = 1u << 4,
    BindRenderTarget  = 1u << 5,
    BindDepthStencil  = 1u << 6,
    BindUnorderedAccess = 1u << 7,
};

enum class MapType : uint32_t {
    Read             = 1,
    Write            = 2,
    ReadWrite        = 3,
    WriteDiscard     = 4,
    WriteNoOverwrite = 5,
};

enum class ShaderStage : uint8_t {
    Vertex, Hull, Domain, Geometry, Pixel, Compute, Unknown
};

// --- Descriptors -----------------------------------------------------------
struct BufferDesc {
    uint32_t byteWidth           = 0;
    Usage    usage               = Usage::Default;
    uint32_t bindFlags           = 0;
    uint32_t cpuAccessFlags      = 0;
    uint32_t miscFlags           = 0;
    uint32_t structureByteStride = 0;
};

struct Texture2DDesc {
    uint32_t width          = 0;
    uint32_t height         = 0;
    uint16_t mipLevels      = 1;
    uint16_t arraySize      = 1;
    uint32_t format         = 0;
    uint32_t sampleCount    = 1;
    uint32_t sampleQuality  = 0;
    Usage    usage          = Usage::Default;
    uint32_t bindFlags      = 0;
    uint32_t cpuAccessFlags = 0;
    uint32_t miscFlags      = 0;
};

struct ShaderDesc {
    ShaderStage stage     = ShaderStage::Unknown;
    const void* bytecode  = nullptr;
    size_t      byteLength= 0;
};

// --- Minimal COM interfaces (implemented in sibling .cpp files) ------------
struct ID3D11DeviceChild {
    virtual ~ID3D11DeviceChild() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
};
using ID3D11Resource = ID3D11DeviceChild;

struct ID3D11Buffer : ID3D11DeviceChild {
    virtual void GetDesc(BufferDesc* out) = 0;
    virtual HRESULT Map(MapType type, uint32_t flags, void** data) = 0;
    virtual void Unmap() = 0;
};

struct ID3D11Texture2D : ID3D11DeviceChild {
    virtual void GetDesc(Texture2DDesc* out) = 0;
    virtual HRESULT Map(uint32_t subresource, MapType type, uint32_t flags,
                        void** data) = 0;
    virtual void Unmap(uint32_t subresource) = 0;
};

// --- Trampolines (defined in sibling .cpp, called from d3d11_device.cpp) ---
extern "C" ID3D11Buffer*    d3d11_buffer_create(void* devicePtr, const BufferDesc& desc);
extern "C" ID3D11Texture2D* d3d11_texture_create(void* devicePtr, const Texture2DDesc& desc);
extern "C" void*            d3d11_shader_create(void* devicePtr, const ShaderDesc& desc);

} // namespace dxvk::d3d11
