// SPDX-License-Identifier: MIT
//
// d3d9_state.cpp — D3D9 state-block / state-cache implementation.
//
// D3D9 exposes `IDirect3DStateBlock9` (a *capture* of device state that can be
// applied atomically) and the live render-state / sampler-state / viewport
// set on the device itself. This file implements both:
//
//   * `D3D9StateCache`     — the live cache the device writes through, with
//                            dirty bits so the Vulkan command stream only
//                            re-issues dynamic state that changed.
//   * `D3D9StateBlockImpl` — a snapshot captured by `IDirect3DStateBlock9`
//                            semantics: `Capture()` reads from a cache,
//                            `Apply()` writes back.
//
// State values are translated to Vulkan dynamic-state enums lazily by the
// device when a draw is dispatched; the cache only stores the raw D3D9 values
// and dirty bits.

#include "vulkan_loader.h"
#include "dxvk_device.h"

#include <cstdint>
#include <cstring>
#include <memory>

namespace dxvk::d3d9 {

// D3DRS_* indices we care about (subset of the real D3DRENDERSTATETYPE enum).
enum RenderStateType : uint32_t {
    D3DRS_ZENABLE           = 7,
    D3DRS_FILLMODE          = 8,
    D3DRS_SHADEMODE         = 9,
    D3DRS_ZWRITEENABLE      = 14,
    D3DRS_ALPHATESTENABLE   = 15,
    D3DRS_SRCBLEND          = 19,
    D3DRS_DESTBLEND         = 20,
    D3DRS_CULLMODE          = 22,
    D3DRS_ZFUNC             = 23,
    D3DRS_ALPHABLENDENABLE  = 27,
    D3DRS_ALPHAFUNC         = 25,
    D3DRS_STENCILZFAIL      = 53,
    D3DRS_STENCILPASS       = 54,
    D3DRS_STENCILFUNC       = 56,
    D3DRS_STENCILREF        = 57,
    D3DRS_STENCILMASK       = 58,
    D3DRS_STENCILWRITEMASK  = 59,
    D3DRS_VIEWPORTENABLE    = 99, // AfriOS extension
    kRenderStateCount       = 256,
};

// D3DSamplerStateType subset.
enum SamplerStateType : uint32_t {
    D3DSAMP_ADDRESSU       = 1,
    D3DSAMP_ADDRESSV       = 2,
    D3DSAMP_ADDRESSW       = 3,
    D3DSAMP_BORDERCOLOR    = 4,
    D3DSAMP_MAGFILTER      = 5,
    D3DSAMP_MINFILTER      = 6,
    D3DSAMP_MIPFILTER      = 7,
    D3DSAMP_MIPMAPLODBIAS  = 8,
    D3DSAMP_MAXMIPLEVEL    = 9,
    D3DSAMP_MAXANISOTROPY  = 10,
    D3DSAMP_SRGBTEXTURE    = 11,
    kSamplerStateCount     = 16,
};

/// D3D9StateCache — live render-state / sampler-state / viewport cache.
class D3D9StateCache {
public:
    D3D9StateCache() { reset(); }

    void reset() noexcept {
        std::memset(m_renderStates, 0, sizeof(m_renderStates));
        std::memset(m_samplerStates, 0, sizeof(m_samplerStates));
        std::memset(&m_viewport, 0, sizeof(m_viewport));
        std::memset(m_clipPlanesEnabled, 0, sizeof(m_clipPlanesEnabled));
        m_renderDirty   = 0;
        m_samplerDirty  = 0;
        m_viewportDirty = false;
        m_clipDirty     = false;
    }

    HRESULT setRenderState(uint32_t state, uint32_t value) noexcept {
        if (state >= kRenderStateCount) return E_INVALIDARG;
        if (m_renderStates[state] != value) {
            m_renderStates[state] = value;
            m_renderDirty |= (1ULL << (state & 63u));
            m_anyDirty = true;
        }
        return S_OK;
    }
    HRESULT getRenderState(uint32_t state, uint32_t& out) const noexcept {
        if (state >= kRenderStateCount) return E_INVALIDARG;
        out = m_renderStates[state];
        return S_OK;
    }

    HRESULT setSamplerState(uint32_t stage, uint32_t type, uint32_t value) noexcept {
        if (stage >= kMaxSamplerStages || type >= kSamplerStateCount) return E_INVALIDARG;
        if (m_samplerStates[stage][type] != value) {
            m_samplerStates[stage][type] = value;
            m_samplerDirty |= (1ULL << stage);
            m_anyDirty = true;
        }
        return S_OK;
    }

    void setViewport(const VkViewport& vp) noexcept {
        std::memcpy(&m_viewport, &vp, sizeof(vp));
        m_viewportDirty = true;
        m_anyDirty = true;
    }
    const VkViewport& viewport() const noexcept { return m_viewport; }

    HRESULT setClipPlane(uint32_t idx, BOOL enable) noexcept {
        if (idx >= kMaxClipPlanes) return E_INVALIDARG;
        m_clipPlanesEnabled[idx] = enable;
        m_clipDirty = true;
        m_anyDirty  = true;
        return S_OK;
    }

    /// True if any state changed since the last `clearDirty()`.
    bool isDirty() const noexcept { return m_anyDirty; }
    void clearDirty() noexcept {
        m_renderDirty = m_samplerDirty = 0;
        m_viewportDirty = m_clipDirty = m_anyDirty = false;
    }

    uint64_t renderDirtyMask()  const noexcept { return m_renderDirty; }
    uint64_t samplerDirtyMask() const noexcept { return m_samplerDirty; }

    /// Direct read access for state-block capture. Pointer is valid for the
    /// lifetime of the cache; the caller must not retain across mutations.
    const uint32_t* renderStatesArray() const noexcept { return m_renderStates; }

private:
    static constexpr uint32_t kMaxSamplerStages = 8;
    static constexpr uint32_t kMaxClipPlanes    = 6;

    uint32_t  m_renderStates[kRenderStateCount] = {};
    uint32_t  m_samplerStates[kMaxSamplerStages][kSamplerStateCount] = {};
    VkViewport m_viewport = {};
    BOOL      m_clipPlanesEnabled[kMaxClipPlanes] = {};

    uint64_t m_renderDirty  = 0;
    uint64_t m_samplerDirty = 0;
    bool     m_viewportDirty = false;
    bool     m_clipDirty    = false;
    bool     m_anyDirty     = false;
};

/// D3D9StateBlockImpl — snapshot used to implement IDirect3DStateBlock9.
class D3D9StateBlockImpl {
public:
    explicit D3D9StateBlockImpl(D3D9StateCache& src) : m_src(src) {}

    void capture() noexcept {
        // Copy live state into our snapshot.
        std::memcpy(m_renderSnapshot, m_src.renderStatesArray(),
                    sizeof(m_renderSnapshot));
        m_captured = true;
    }
    void apply() noexcept {
        if (!m_captured) return;
        for (uint32_t s = 0; s < kRenderStateCount; ++s) {
            m_src.setRenderState(s, m_renderSnapshot[s]);
        }
    }

private:
    static constexpr uint32_t kRenderStateCount = 256;
    D3D9StateCache& m_src;
    uint32_t m_renderSnapshot[kRenderStateCount] = {};
    bool     m_captured = false;
};

} // namespace dxvk::d3d9
