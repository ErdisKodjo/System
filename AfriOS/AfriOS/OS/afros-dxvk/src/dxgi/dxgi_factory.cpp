// SPDX-License-Identifier: MIT
//
// dxgi_factory.cpp — IDXGIFactory implementation.
//
// DXGI is the lowest layer a Windows game talks to before D3D; the factory is
// the root object from which adapters (`IDXGIAdapter`) and swapchains
// (`IDXGISwapChain`) are obtained. We back the factory with the AfriOS
// `DxvkAdapter` list (typically one primary GPU) and the shared `DxvkDevice`
// singleton. `CreateSwapChain()` validates the incoming desc, creates a
// Vulkan surface from `outputWindow` (Win32 / Wayland / X11), and forwards to
// the sibling `dxgi_swapchain_create` trampoline.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"
#include "dxgi_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk::dxgi {

/// DxgiFactoryImpl — concrete IDXGIFactory.
class DxgiFactoryImpl : public IDXGIFactory {
public:
    DxgiFactoryImpl(std::shared_ptr<DxvkAdapter> primaryAdapter,
                    std::shared_ptr<DxvkDevice> device)
        : m_primaryAdapter(std::move(primaryAdapter))
        , m_device(std::move(device)) {
        if (m_primaryAdapter) m_adapters.push_back(m_primaryAdapter);
    }

    HRESULT QueryInterface(const void*, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override  { return ++m_refCount; }
    uint32_t Release() override {
        auto n = --m_refCount; if (n == 0) delete this; return n;
    }

    HRESULT EnumAdapters(uint32_t index, IDXGIAdapter** out) override {
        if (!out) return E_POINTER;
        if (index >= m_adapters.size()) return DXGI_ERROR_INVALID_CALL;
        *out = dxgi_adapter_create(m_adapters[index]);
        return *out ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT CreateSwapChain(void* /*device*/, const SwapChainDesc* desc,
                            IDXGISwapChain** out) override {
        if (!desc || !out) return E_POINTER;
        if (desc->bufferWidth == 0 || desc->bufferHeight == 0)
            return E_INVALIDARG;
        if (!m_device) return E_FAIL;
        // Real impl also creates a VkSurfaceKHR from desc->outputWindow here
        // and passes it into the swapchain. The trampoline only needs the
        // device + desc for the skeleton.
        *out = dxgi_swapchain_create(m_device, *desc);
        return *out ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT MakeWindowAssociation(void* /*window*/, uint32_t /*flags*/) override {
        // Real impl routes to X11/Wayland alt-tab + fullscreen handling.
        return S_OK;
    }

    /// Used by AfriOS boot to register additional adapters (e.g. a software
    /// fallback) after the factory has been created.
    void registerAdapter(std::shared_ptr<DxvkAdapter> adapter) {
        if (adapter) m_adapters.push_back(std::move(adapter));
    }

    uint32_t adapterCount() const noexcept {
        return static_cast<uint32_t>(m_adapters.size());
    }

private:
    std::shared_ptr<DxvkAdapter> m_primaryAdapter;
    std::shared_ptr<DxvkDevice>  m_device;
    std::vector<std::shared_ptr<DxvkAdapter>> m_adapters;
    uint32_t m_refCount = 1;
};

} // namespace dxvk::dxgi

// --- C entry points exported to Wine ---------------------------------------
extern "C" {

/// CreateDXGIFactory — the DXGI factory entry point Wine resolves by name.
/// `riid` is ignored; we always return an IDXGIFactory.
dxvk::HRESULT CreateDXGIFactory(uint32_t /*riid*/, void** out) {
    if (!out) return dxvk::E_POINTER;
    // Real impl builds a VulkanLoader, picks the first VkPhysicalDevice, wraps
    // it in a DxvkAdapter, then constructs a DxvkDevice. The skeleton returns
    // E_FAIL when no ICD is present so callers can fall back gracefully.
    *out = nullptr;
    return dxvk::E_FAIL;
}

/// CreateDXGIFactory1 — same as above but guaranteed to expose IDXGIFactory1
/// (we expose the same implementation).
dxvk::HRESULT CreateDXGIFactory1(uint32_t /*riid*/, void** out) {
    return CreateDXGIFactory(0, out);
}

/// AfriOS-internal helper used by the boot glue to construct a factory when
/// the Vulkan loader + primary adapter are already known.
extern "C" dxvk::dxgi::IDXGIFactory* dxgi_factory_create_internal(
    std::shared_ptr<dxvk::DxvkAdapter> adapter,
    std::shared_ptr<dxvk::DxvkDevice>  device) {
    return new dxvk::dxgi::DxgiFactoryImpl(std::move(adapter), std::move(device));
}

} // extern "C"
