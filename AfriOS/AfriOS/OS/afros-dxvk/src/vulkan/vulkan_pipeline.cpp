// SPDX-License-Identifier: MIT
//
// vulkan_pipeline.cpp — graphics / compute pipeline cache.
//
// Each unique combination of shader modules + vertex input + blend / raster /
// depth / multisample / dynamic state maps to one `VkPipeline`. The cache
// memoizes these by a hash of the full state key so the (expensive)
// `vkCreateGraphicsPipelines` call happens at most once per key. The on-disk
// `VkPipelineCache` is loaded at startup (see util/cache_manager.cpp) and
// flushed periodically so pipelines survive process restarts.

#include "vulkan_loader.h"
#include "dxvk_device.h"
#include "vulkan_private.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace dxvk {

/// Hashed key identifying one pipeline. The skeleton hashes a small descriptor
/// blob; the real build hashes the full DxvkGraphicsPipelineState.
struct PipelineKey {
    uint64_t shaderHash  = 0; // hash of all bound shader modules
    uint32_t vertexStride= 0;
    uint32_t topology    = 0; // VkPrimitiveTopology
    uint32_t blendState  = 0; // packed blend enables / write masks
    uint32_t rasterState = 0; // packed cull / fill / front-face
    uint32_t depthState  = 0; // packed depth test / write / compare
    uint32_t sampleMask  = 0;

    bool operator==(const PipelineKey& o) const noexcept {
        return shaderHash   == o.shaderHash
            && vertexStride == o.vertexStride
            && topology     == o.topology
            && blendState   == o.blendState
            && rasterState  == o.rasterState
            && depthState   == o.depthState
            && sampleMask   == o.sampleMask;
    }
};

struct PipelineKeyHash {
    size_t operator()(const PipelineKey& k) const noexcept {
        // FNV-1a 64 over the key bytes.
        uint64_t h = 1469598103934665603ull;
        const auto* p = reinterpret_cast<const uint8_t*>(&k);
        for (size_t i = 0; i < sizeof(k); ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
        return static_cast<size_t>(h);
    }
};

/// VulkanPipelineCache — owns one `VkPipelineCache` + a map of compiled
/// pipelines keyed by `PipelineKey`.
class VulkanPipelineCache {
public:
    explicit VulkanPipelineCache(std::shared_ptr<DxvkDevice> device)
        : m_device(std::move(device)) {
        m_pipelineCache = m_device->createPipelineCache();
    }
    ~VulkanPipelineCache() {
        for (auto& kv : m_graphics) if (kv.second) m_device->destroyPipeline(kv.second);
        for (auto& kv : m_compute)  if (kv.second) m_device->destroyPipeline(kv.second);
        m_device->destroyPipelineCache(m_pipelineCache);
    }

    /// Look up or create a graphics pipeline for the given state key. The
    /// shader-stage / vertex-input / blend / etc. CreateInfo structs are
    /// assembled from `key` + `stages` and passed to vkCreateGraphicsPipelines.
    VkPipeline createGraphicsPipelines(const PipelineKey& key,
                                       const std::vector<VkShaderModule>& stages) {
        const auto it = m_graphics.find(key);
        if (it != m_graphics.end()) return it->second;
        VkPipeline p = compileGraphics(key, stages);
        m_graphics.emplace(key, p);
        return p;
    }

    /// Look up or create a compute pipeline.
    VkPipeline createComputePipelines(uint64_t shaderHash, VkShaderModule shader) {
        PipelineKey key{};
        key.shaderHash = shaderHash;
        const auto it = m_compute.find(key);
        if (it != m_compute.end()) return it->second;
        VkPipeline p = compileCompute(shader);
        m_compute.emplace(key, p);
        return p;
    }

    size_t graphicsCount() const noexcept { return m_graphics.size(); }
    size_t computeCount()  const noexcept { return m_compute.size(); }

    /// Serialize the underlying VkPipelineCache to disk (delegates to
    /// util/cache_manager.cpp in a real build).
    void flush() noexcept {
        // vkGetPipelineCacheData(m_device->device(), m_pipelineCache, &size, data)
        // → write to /var/cache/afros-dxvk/pipeline.bin
    }

private:
    VkPipeline compileGraphics(const PipelineKey& /*key*/,
                               const std::vector<VkShaderModule>& /*stages*/) {
        // Real impl: assemble VkGraphicsPipelineCreateInfo with VkPipelineShaderStageCreateInfo
        // for each stage, VkPipelineVertexInputStateCreateInfo, VkPipelineInputAssemblyStateCreateInfo
        // (topology), VkPipelineColorBlendStateCreateInfo (blendState), VkPipelineRasterizationStateCreateInfo
        // (rasterState), VkPipelineDepthStencilStateCreateInfo (depthState), then call
        // vkCreateGraphicsPipelines(m_device->device(), m_pipelineCache, 1, &ci, nullptr, &p).
        // The skeleton returns a sentinel handle so the device-context bind path has something to carry.
        return reinterpret_cast<VkPipeline>(static_cast<uintptr_t>(0x1));
    }
    VkPipeline compileCompute(VkShaderModule /*shader*/) {
        // Real impl: VkComputePipelineCreateInfo + vkCreateComputePipelines.
        return reinterpret_cast<VkPipeline>(static_cast<uintptr_t>(0x2));
    }

    std::shared_ptr<DxvkDevice> m_device;
    VkPipelineCache             m_pipelineCache = nullptr;
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> m_graphics;
    std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> m_compute;
};

} // namespace dxvk

// --- C entry points used by the device-context layer -----------------------
extern "C" {

dxvk::VulkanPipelineCache* vkpipe_create(void* devicePtr) {
    auto device = *static_cast<std::shared_ptr<dxvk::DxvkDevice>*>(devicePtr);
    return new dxvk::VulkanPipelineCache(std::move(device));
}
void vkpipe_destroy(dxvk::VulkanPipelineCache* cache) { delete cache; }

} // extern "C"
