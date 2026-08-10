// SPDX-License-Identifier: MIT
//
// dxgi_types.h — shared DXGI desc / interface declarations used by every
// dxgi translation unit. Keeps each .cpp self-contained while letting the
// factory construct adapter / swapchain wrappers defined in sibling files.

#pragma once

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"

#include <cstdint>
#include <memory>

namespace dxvk::dxgi {

/// DXGI_ADAPTER_DESC (subset).
struct AdapterDesc {
    char     description[128] = {};
    uint32_t vendorId  = 0;
    uint32_t deviceId  = 0;
    uint32_t subSysId  = 0;
    uint32_t revision  = 0;
    uint64_t dedicatedVideoMemory = 0;
    uint64_t dedicatedSystemMemory = 0;
    uint64_t sharedSystemMemory = 0;
    uint64_t adapterLuidLow = 0;
    int64_t  adapterLuidHigh = 0;
};

/// DXGI_SWAP_CHAIN_DESC (subset).
struct SwapChainDesc {
    uint32_t bufferWidth   = 0;
    uint32_t bufferHeight  = 0;
    uint32_t bufferFormat  = 0;
    uint32_t bufferUsage   = 0;
    uint32_t bufferCount   = 2;
    uint32_t swapEffect    = 0;
    void*    outputWindow  = nullptr;
    BOOL     windowed      = TRUE;
    uint32_t sampleCount   = 1;
    uint32_t sampleQuality = 0;
    uint32_t syncInterval  = 1; // 0 = tearing, 1+ = vsync
    uint32_t presentFlags  = 0;
};

/// Minimal IDXGIObject base.
struct IDXGIObject {
    virtual ~IDXGIObject() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
};

struct IDXGIAdapter : IDXGIObject {
    virtual HRESULT GetDesc(AdapterDesc* out) = 0;
    virtual HRESULT EnumOutputs(uint32_t index, void** out) = 0;
};

struct IDXGISwapChain : IDXGIObject {
    virtual HRESULT Present(uint32_t syncInterval, uint32_t flags) = 0;
    virtual HRESULT ResizeBuffers(uint32_t bufferCount, uint32_t width,
                                  uint32_t height, uint32_t format,
                                  uint32_t flags) = 0;
    virtual HRESULT GetBuffer(uint32_t index, const void* iid,
                              void** out) = 0;
    virtual HRESULT GetDesc(SwapChainDesc* out) = 0;
};

struct IDXGIFactory : IDXGIObject {
    virtual HRESULT EnumAdapters(uint32_t index, IDXGIAdapter** out) = 0;
    virtual HRESULT CreateSwapChain(void* device, const SwapChainDesc* desc,
                                    IDXGISwapChain** out) = 0;
    virtual HRESULT MakeWindowAssociation(void* window, uint32_t flags) = 0;
};

// --- Trampolines (defined in sibling .cpp, called from dxgi_factory.cpp) ---
extern "C" IDXGIAdapter*  dxgi_adapter_create(std::shared_ptr<DxvkAdapter> adapter);
extern "C" IDXGISwapChain* dxgi_swapchain_create(std::shared_ptr<DxvkDevice> device,
                                                 const SwapChainDesc& desc);

} // namespace dxvk::dxgi
