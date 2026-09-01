#pragma once

/**
 * @file
 * @brief Stateful ONNX Runtime execution of the Phase 3 fuzz models.
 *
 * The trained model is a causal 2-layer GRU (one per clip branch) whose
 * graph contract is:
 *   x    float32 [1, T]   audio block
 *   cond float32 [1, 4]   [fuzz, tone, high, hilo]
 *   h    float32 [L, 1, H] stacked per-layer state  (L=2, H=24)
 *   -> y float32 [1, T], h_out float32 [L, 1, H]
 *
 * Parked: the plugin is DSP-only. This module is compiled only into
 * NeuralTests when FA_ENABLE_ONNX is on.
 *
 * The session runs on the audio thread per block, carrying h across calls
 * (the statefulness is what models envelope/sustain dynamics — PLAN.md
 * Section 2 pitfall #2). All Ort::Value tensors wrap preallocated buffers so
 * the audio thread never allocates.
 */

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#if defined(FA_ENABLE_ONNX)
#include <onnxruntime_cxx_api.h>
#endif

namespace fairo
{

class FuzzNeuralInference
{
public:
    enum class Branch { Silicon, Germanium, Bypass };

    FuzzNeuralInference() = default;
    ~FuzzNeuralInference() = default;

    /** @brief Load the model for @a branch from an embedded byte blob.
     *  Returns true on success. Safe to call again (reload). */
    bool loadModel(Branch branch, const void* data, size_t size);

    /** @brief Load from a model-file path (dev/test convenience). */
    bool loadModelFile(Branch branch, const char* path);

    /** @brief Process one mono block. cond = [fuzz, tone, high, hilo] (0..1).
     *  State carries across calls; call reset() to clear. */
    void processBlock(const float* in, float* out, int numSamples,
                      const float cond[4]) noexcept;

    /** @brief Clear the input history ring buffer (call on prepare/reset). */
    void reset() noexcept;

    /** @brief Receptive field of the trained model (samples). */
    static constexpr int kContextSamples = 4096;

    bool isValid() const noexcept
    {
#if defined(FA_ENABLE_ONNX)
        return session_ != nullptr;
#else
        return false;
#endif
    }

private:
#if defined(FA_ENABLE_ONNX)
    Ort::Env& env();
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memInfo_{
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault) };
    std::array<const char*, 2> inputNames_{ "x", "cond" };
    std::array<const char*, 1> outputNames_{ "y" };

    std::vector<float> histBuf_;   // kContextSamples + maxBlock (ring input history)
    std::vector<float> yBuf_;      // 1*block
#endif
};

} // namespace fairo
