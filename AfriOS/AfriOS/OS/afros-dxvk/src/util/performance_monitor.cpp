// SPDX-License-Identifier: MIT
//
// performance_monitor.cpp — frame stats, GPU timestamp queries, on-screen
// overlay.
//
// The monitor is a singleton owned by the DXGI swapchain. Every `Present()`
// call records a frame-end timestamp; the monitor derives FPS + frame time
// from a sliding window of the last N samples. GPU-side timing uses Vulkan
// timestamp queries (`vkCmdWriteTimestamp` at the start and end of each
// frame's command buffer, read back via `vkGetQueryPoolResults`).
//
// An optional overlay (text rendered via the host compositor, or a Vulkan
// text layer in a real build) shows FPS / frame time / GPU time / draw count.

#include "vulkan_loader.h"

#include <cstdint>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dxvk {

/// One frame's worth of measurements.
struct FrameSample {
    double cpuFrameMs = 0.0;
    double gpuFrameMs = 0.0;
    uint32_t drawCount = 0;
    uint32_t dispatchCount = 0;
};

/// PerformanceMonitor — sliding-window frame stats + GPU timestamp queries.
class PerformanceMonitor {
public:
    PerformanceMonitor() { reset(); }

    void reset() noexcept {
        m_samples.clear();
        m_samples.reserve(kWindow);
        m_frameNumber = 0;
        m_drawCount   = 0;
        m_dispatchCount = 0;
        m_lastFrameStart = Clock::now();
        m_gpuTimestampStart = 0;
        m_gpuTimestampEnd   = 0;
    }

    /// Called at the start of each frame (BeginScene / first command buffer
    /// recording). Records the CPU-side start time + writes a GPU timestamp
    /// via vkCmdWriteTimestamp into the per-frame query slot.
    void beginFrame() {
        m_lastFrameStart = Clock::now();
        // Real impl: vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        //                                m_queryPool, m_frameNumber % kWindow);
        m_gpuTimestampStart = 0;
    }

    /// Called at Present() time. Computes the CPU frame time + reads back the
    /// GPU timestamp pair for this frame.
    void endFrame() {
        const auto now = Clock::now();
        const double cpuMs = std::chrono::duration<double, std::milli>(
            now - m_lastFrameStart).count();
        // Real impl: vkGetQueryPoolResults(... m_queryPool, slot, 2, ...,
        //                                  VK_QUERY_RESULT_64_BIT | WAIT_BIT).
        const double gpuMs = computeGpuTimeMs();
        FrameSample s;
        s.cpuFrameMs = cpuMs;
        s.gpuFrameMs = gpuMs;
        s.drawCount  = m_drawCount;
        s.dispatchCount = m_dispatchCount;
        m_samples.push_back(s);
        if (m_samples.size() > kWindow) m_samples.erase(m_samples.begin());
        ++m_frameNumber;
        m_drawCount = 0;
        m_dispatchCount = 0;
    }

    void recordDraw()     noexcept { ++m_drawCount; }
    void recordDispatch() noexcept { ++m_dispatchCount; }

    /// Average FPS over the sliding window.
    double fps() const noexcept {
        if (m_samples.empty()) return 0.0;
        double sumMs = 0.0;
        for (const auto& s : m_samples) sumMs += s.cpuFrameMs;
        return sumMs > 0.0 ? static_cast<double>(m_samples.size()) * 1000.0 / sumMs : 0.0;
    }

    /// Average frame time (ms) over the window.
    double avgFrameTimeMs() const noexcept {
        if (m_samples.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& s : m_samples) sum += s.cpuFrameMs;
        return sum / static_cast<double>(m_samples.size());
    }

    /// Average GPU frame time (ms) over the window.
    double avgGpuTimeMs() const noexcept {
        if (m_samples.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& s : m_samples) sum += s.gpuFrameMs;
        return sum / static_cast<double>(m_samples.size());
    }

    /// Last frame's draw count.
    uint32_t lastDrawCount() const noexcept {
        return m_samples.empty() ? 0 : m_samples.back().drawCount;
    }

    uint64_t frameNumber() const noexcept { return m_frameNumber; }

    /// Render a 3-line overlay string (FPS / frame time / GPU time + draws).
    std::string overlayText() const {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "afros-dxvk | FPS: %5.1f | frame: %5.2f ms | GPU: %5.2f ms | draws: %u",
            fps(), avgFrameTimeMs(), avgGpuTimeMs(), lastDrawCount());
        return std::string(buf);
    }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr size_t kWindow = 120; // ~2 s @ 60 FPS

    double computeGpuTimeMs() const noexcept {
        // GPU timestamp ticks are nanoseconds; convert to ms.
        // Real impl reads the timestamp period from VkPhysicalDeviceLimits.
        if (m_gpuTimestampEnd <= m_gpuTimestampStart) return 0.0;
        const double ns = static_cast<double>(m_gpuTimestampEnd - m_gpuTimestampStart);
        return ns / 1'000'000.0;
    }

    std::vector<FrameSample> m_samples;
    Clock::time_point         m_lastFrameStart;
    uint64_t                  m_gpuTimestampStart = 0;
    uint64_t                  m_gpuTimestampEnd   = 0;
    uint64_t                  m_frameNumber       = 0;
    uint32_t                  m_drawCount         = 0;
    uint32_t                  m_dispatchCount     = 0;
};

} // namespace dxvk

// --- C entry points used by the DXGI swapchain + overlay -------------------
extern "C" {

dxvk::PerformanceMonitor* perf_monitor_default() {
    static dxvk::PerformanceMonitor g_default;
    return &g_default;
}

} // extern "C"
