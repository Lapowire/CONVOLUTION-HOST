#pragma once
#include "RoutingModel.h"
#include "ir/PreparedIr.h"
#include <juce_dsp/juce_dsp.h>
namespace ch
{
class ConvolutionEngine
{
  public:
    ConvolutionEngine(std::shared_ptr<const PreparedIr> ir, RoutingModel routing);
    void prepare(double sampleRate, int maxBlockSize, int channels);
    void process(juce::AudioBuffer<float> &) noexcept;
    void reset() noexcept;
    int latencySamples() const noexcept { return 0; }
    double durationSeconds() const noexcept { return impulse->durationSeconds(); }
    const PreparedIr &ir() const { return *impulse; }

  private:
    struct Path
    {
        Route route;
        std::unique_ptr<juce::dsp::Convolution> convolution;
    };
    std::shared_ptr<const PreparedIr> impulse;
    RoutingModel model;
    juce::dsp::ConvolutionMessageQueue queue{64};
    std::vector<Path> paths;
    juce::AudioBuffer<float> input, scratch;
    int blockSize = 0, channelCount = 0;
};
} // namespace ch
