#include "FuzzNeuralInference.h"

#include <algorithm>
#include <cstring>

namespace fairo
{

#if defined(FA_ENABLE_ONNX)

Ort::Env& FuzzNeuralInference::env()
{
    static Ort::Env ortEnv(ORT_LOGGING_LEVEL_WARNING, "FairoNeural");
    return ortEnv;
}

bool FuzzNeuralInference::loadModel(Branch, const void* data, size_t size)
{
    try
    {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetInterOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(env(), data, size, opts);
        histBuf_.assign(static_cast<size_t>(kContextSamples), 0.0f);
        return true;
    }
    catch (const Ort::Exception&)
    {
        session_.reset();
        return false;
    }
}

bool FuzzNeuralInference::loadModelFile(Branch branch, const char* path)
{
    try
    {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(2);
        opts.SetInterOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        std::string p(path != nullptr ? path : "");
        session_ = std::make_unique<Ort::Session>(env(), p.c_str(), opts);
        histBuf_.assign(static_cast<size_t>(kContextSamples), 0.0f);
        return true;
    }
    catch (const Ort::Exception&)
    {
        session_.reset();
        return false;
    }
}

void FuzzNeuralInference::reset() noexcept
{
    std::fill(histBuf_.begin(), histBuf_.end(), 0.0f);
}

void FuzzNeuralInference::processBlock(const float* in, float* out, int numSamples,
                                       const float cond[4]) noexcept
{
    if (session_ == nullptr || numSamples <= 0 || in == nullptr || out == nullptr)
        return;

    try
    {
        const int needed = kContextSamples + numSamples;
        if (static_cast<int>(histBuf_.size()) < needed)
            histBuf_.resize(static_cast<size_t>(needed));
        if (static_cast<int>(yBuf_.size()) < needed)
            yBuf_.resize(static_cast<size_t>(needed));

        // Slide the history window: keep the last kContextSamples, append the
        // new block (the model's causal context spans call boundaries).
        const int tail = kContextSamples;
        std::copy(histBuf_.begin() + numSamples, histBuf_.begin() + tail + numSamples,
                  histBuf_.begin());
        std::copy(in, in + numSamples, histBuf_.begin() + tail);

        const std::array<int64_t, 2> xShape{ 1, static_cast<int64_t>(tail + numSamples) };
        const std::array<int64_t, 2> cShape{ 1, 4 };
        const std::array<int64_t, 2> yShape{ 1, static_cast<int64_t>(tail + numSamples) };

        Ort::Value inTensors[] = {
            Ort::Value::CreateTensor<float>(memInfo_, histBuf_.data(),
                                            static_cast<size_t>(tail + numSamples),
                                            xShape.data(), xShape.size()),
            Ort::Value::CreateTensor<float>(memInfo_, const_cast<float*>(cond), 4,
                                            cShape.data(), cShape.size()),
        };
        Ort::Value outTensors[] = {
            Ort::Value::CreateTensor<float>(memInfo_, yBuf_.data(),
                                            static_cast<size_t>(tail + numSamples),
                                            yShape.data(), yShape.size()),
        };

        session_->Run(Ort::RunOptions{ nullptr }, inputNames_.data(), inTensors, 2,
                      outputNames_.data(), outTensors, 1);

        // The model is causal and returns [context + block]; keep the current
        // block (the last numSamples) as this call's output.
        std::memcpy(out, yBuf_.data() + tail,
                    static_cast<size_t>(numSamples) * sizeof(float));
    }
    catch (const Ort::Exception&)
    {
        // Never propagate exceptions from the audio thread.
        if (out != in)
            std::memcpy(out, in, static_cast<size_t>(numSamples) * sizeof(float));
    }
}

#else  // !FA_ENABLE_ONNX

bool FuzzNeuralInference::loadModel(Branch, const void*, size_t) { return false; }
bool FuzzNeuralInference::loadModelFile(Branch, const char*) { return false; }
void FuzzNeuralInference::processBlock(const float* in, float* out, int n, const float[]) noexcept
{
    if (out != in && in != nullptr && out != nullptr)
        std::memcpy(out, in, static_cast<size_t>(n) * sizeof(float));
}
void FuzzNeuralInference::reset() noexcept {}

#endif

} // namespace fairo
