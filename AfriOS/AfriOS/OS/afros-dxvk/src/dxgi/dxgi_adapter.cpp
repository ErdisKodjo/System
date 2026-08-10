// SPDX-License-Identifier: MIT
//
// dxgi_adapter.cpp — IDXGIAdapter implementation wrapping a DxvkAdapter.
//
// DXGI exposes physical GPUs to the application via `IDXGIAdapter`. Each
// adapter's `GetDesc()` returns vendor/device IDs, dedicated video memory,
// and a human-readable description (surfaced from `DxvkAdapter::properties()`).
// `EnumOutputs()` enumerates the adapter's attached displays; AfriOS exposes a
// single virtual output per adapter for now.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxgi_types.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk::dxgi {

/// Minimal IDXGIOutput stub (single virtual output per adapter).
struct IDXGIOutput : IDXGIObject {
    virtual HRESULT GetDesc(void* out) = 0;
    virtual HRESULT GetDisplayModeList(uint32_t format, uint32_t flags,
                                       uint32_t* count, void* modes) = 0;
};

class DxgiOutputImpl : public IDXGIOutput {
public:
    explicit DxgiOutputImpl(std::shared_ptr<DxvkAdapter> adapter)
        : m_adapter(std::move(adapter)) {}
    HRESULT QueryInterface(const void*, void** out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        return E_NOINTERFACE;
    }
    uint32_t AddRef() override  { return ++m_refCount; }
    uint32_t Release() override {
        auto n = --m_refCount; if (n == 0) delete this; return n;
    }
    HRESULT GetDesc(void* /*out*/) override { return E_NOTIMPL; }
    HRESULT GetDisplayModeList(uint32_t /*format*/, uint32_t /*flags*/,
                               uint32_t* count, void* /*modes*/) override {
        if (!count) return E_POINTER;
        *count = 1; // single default mode
        return S_OK;
    }
private:
    std::shared_ptr<DxvkAdapter> m_adapter;
    uint32_t m_refCount = 1;
};

/// DxgiAdapterImpl — concrete IDXGIAdapter.
class DxgiAdapterImpl : public IDXGIAdapter {
public:
    explicit DxgiAdapterImpl(std::shared_ptr<DxvkAdapter> adapter)
        : m_adapter(std::move(adapter)) {
        m_desc = AdapterDesc{};
        if (m_adapter) {
            const auto& p = m_adapter->properties();
            std::strncpy(m_desc.description, p.deviceName,
                         sizeof(m_desc.description) - 1);
            m_desc.vendorId  = p.vendorID;
            m_desc.deviceId  = p.deviceID;
            m_desc.dedicatedVideoMemory =
                m_adapter->memory().heaps.empty()
                    ? 0 : m_adapter->memory().heaps.front().size;
        }
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

    HRESULT GetDesc(AdapterDesc* out) override {
        if (!out) return E_POINTER;
        *out = m_desc;
        return S_OK;
    }
    HRESULT EnumOutputs(uint32_t index, void** out) override {
        if (!out || index != 0) return E_INVALIDARG;
        *out = new DxgiOutputImpl(m_adapter);
        return S_OK;
    }

    DxvkAdapter& adapter() const noexcept { return *m_adapter; }

private:
    std::shared_ptr<DxvkAdapter> m_adapter;
    AdapterDesc                  m_desc{};
    uint32_t                     m_refCount = 1;
};

} // namespace dxvk::dxgi

// --- Trampoline used by dxgi_factory.cpp::EnumAdapters ---------------------
extern "C" dxvk::dxgi::IDXGIAdapter* dxgi_adapter_create(
    std::shared_ptr<dxvk::DxvkAdapter> adapter) {
    return new dxvk::dxgi::DxgiAdapterImpl(std::move(adapter));
}
