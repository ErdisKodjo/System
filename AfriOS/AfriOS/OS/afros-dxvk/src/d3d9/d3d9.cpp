// SPDX-License-Identifier: MIT
//
// d3d9.cpp — `Direct3DCreate9()` factory + IDirect3D9 implementation.
//
// This is the D3D9 entry point Wine loads when a Windows application calls
// `Direct3DCreate9(UINT SDKVersion)`. We expose a minimal `IDirect3D9`
// interface whose `CreateDevice()` constructs a `D3D9Device` backed by the
// shared `DxvkDevice`. The other enumeration methods (`GetAdapterCount`,
// `GetAdapterIdentifier`, `GetAdapterModeCount`, ...) read from the
// `DxvkAdapter` list the DXGI factory populated at startup.

#include "vulkan_loader.h"
#include "dxvk_adapter.h"
#include "dxvk_device.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace dxvk {

// --- Forward declarations of sibling D3D9 types ----------------------------
class D3D9Device;

// Trampoline defined in d3d9_device.cpp (constructs the concrete D3D9Device
// without exposing its full declaration to this translation unit).
extern "C" D3D9Device* d3d9_device_create(void* devicePtr);

// Minimal local subset of the IDirect3D9 COM interface. The real Windows
// header declares ~30 methods; we declare the ones the translation layer
// actually implements and route the rest to E_NOTIMPL at the call site.
struct IDirect3D9 {
    virtual ~IDirect3D9() = default;
    virtual HRESULT QueryInterface(const void* iid, void** out) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;

    virtual HRESULT RegisterSoftwareDevice(void* init) = 0;
    virtual uint32_t GetAdapterCount() = 0;
    virtual HRESULT GetAdapterIdentifier(uint32_t adapter, uint32_t flags,
                                         void* identifier) = 0;
    virtual uint32_t GetAdapterModeCount(uint32_t adapter, uint32_t format) = 0;
    virtual HRESULT GetAdapterDisplayMode(uint32_t adapter, void* mode) = 0;
    virtual HRESULT CheckDeviceType(uint32_t adapter, uint32_t devType,
                                    uint32_t fmt, uint32_t bbfmt, BOOL windowed) = 0;
    virtual HRESULT CheckDeviceFormat(uint32_t adapter, uint32_t devType,
                                      uint32_t fmt, uint32_t usage,
                                      uint32_t resType, uint32_t checkFmt) = 0;
    virtual HRESULT CreateDevice(uint32_t adapter, uint32_t devType,
                                 void* focusWindow, uint32_t behaviorFlags,
                                 void* params, D3D9Device** outDevice) = 0;
};

// Minimal D3D9 adapter identifier (subset of D3DADAPTER_IDENTIFIER9).
struct D3D9AdapterIdentifier {
    char     driver[512] = {};
    char     description[512] = {};
    char     deviceName[32] = {};
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint32_t subSysId = 0;
    uint32_t revision = 0;
    uint32_t driverVersion = 0;
};

/// D3D9 — concrete IDirect3D9 implementation.
class D3D9 : public IDirect3D9 {
public:
    explicit D3D9(std::shared_ptr<DxvkDevice> device)
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

    HRESULT RegisterSoftwareDevice(void* /*init*/) override { return E_NOTIMPL; }

    uint32_t GetAdapterCount() override {
        // We expose one logical adapter (the primary physical device).
        return m_device ? 1u : 0u;
    }

    HRESULT GetAdapterIdentifier(uint32_t adapter, uint32_t /*flags*/,
                                 void* identifier) override {
        if (adapter != 0 || !identifier) return E_INVALIDARG;
        auto* id = static_cast<D3D9AdapterIdentifier*>(identifier);
        *id = D3D9AdapterIdentifier{};
        const auto& props = m_device->adapter().properties();
        std::strncpy(id->description, props.deviceName, sizeof(id->description) - 1);
        std::strncpy(id->driver,     "afros-dxvk",      sizeof(id->driver) - 1);
        id->vendorId      = props.vendorID;
        id->deviceId      = props.deviceID;
        id->driverVersion = props.driverVersion;
        return S_OK;
    }

    uint32_t GetAdapterModeCount(uint32_t /*adapter*/, uint32_t /*format*/) override {
        return 1; // single default mode
    }
    HRESULT GetAdapterDisplayMode(uint32_t /*adapter*/, void* /*mode*/) override {
        return S_OK;
    }
    HRESULT CheckDeviceType(uint32_t, uint32_t, uint32_t, uint32_t,
                            BOOL) override { return S_OK; }
    HRESULT CheckDeviceFormat(uint32_t, uint32_t, uint32_t, uint32_t,
                              uint32_t, uint32_t) override { return S_OK; }

    HRESULT CreateDevice(uint32_t adapter, uint32_t devType,
                         void* focusWindow, uint32_t behaviorFlags,
                         void* params, D3D9Device** outDevice) override {
        (void)adapter; (void)devType; (void)focusWindow;
        (void)behaviorFlags; (void)params;
        if (!outDevice) return E_POINTER;
        // Hand off to d3d9_device.cpp which owns the D3D9Device definition.
        *outDevice = d3d9_device_create(&m_device);
        return *outDevice ? S_OK : E_OUTOFMEMORY;
    }

    DxvkDevice& device() noexcept { return *m_device; }

private:
    std::shared_ptr<DxvkDevice> m_device;
    uint32_t                    m_refCount = 1;
};

} // namespace dxvk

// --- C entry point exported to Wine ---------------------------------------
extern "C" {

/// Direct3DCreate9 — the D3D9 factory symbol Wine resolves by name.
/// `SDKVersion` is ignored (D3D9 SDK_VERSION); we always return an adapter
/// bound to the AfriOS DxvkDevice singleton.
dxvk::IDirect3D9* Direct3DCreate9(uint32_t /*SDKVersion*/) {
    // In a fully wired build this would call DxvkDevice::create() with the
    // VulkanLoader singleton. For the standalone stub we return nullptr when
    // no device is available — the caller (Wine) treats this as "no D3D9".
    static std::shared_ptr<dxvk::DxvkDevice> g_device;
    if (!g_device) {
        // Lazy init: a real loader/adapter would be injected here.
        return nullptr;
    }
    return new dxvk::D3D9(g_device);
}

/// Direct3DCreate9Ex — Extended factory; routes to the same path.
dxvk::HRESULT Direct3DCreate9Ex(uint32_t sdk, void** out) {
    if (!out) return dxvk::E_POINTER;
    auto* d3d = Direct3DCreate9(sdk);
    if (!d3d) return dxvk::E_FAIL;
    *out = d3d;
    return dxvk::S_OK;
}

} // extern "C"
