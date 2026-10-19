#pragma once
#include "ir/IrLoader.h"
#include <array>
namespace ch
{
inline constexpr std::array<float, 10> eqFrequencies{31.5f,  63.f,   125.f,  250.f,  500.f,
                                                     1000.f, 2000.f, 4000.f, 8000.f, 16000.f};
struct ParameterSnapshot
{
    float inputGain = 0, outputGain = 0, wetGain = 0, mix = 25, predelay = 0, width = 100;
    std::array<float, 10> eq{};
    float highpass = 20, lowpass = 20000;
    bool filtersEnabled = false, bypass = false;
};
class ReverbProcessor
{
  public:
    void prepare(double sampleRate, int maxBlockSize, int channels);
    void process(juce::AudioBuffer<float> &, const ParameterSnapshot &) noexcept;
    void reset() noexcept;
    IrLoader &loader() { return impulses; }
    const IrLoader &loader() const { return impulses; }
    std::atomic<float> inputPeak{0}, outputPeak{0};

  private:
    struct Filter
    {
        std::array<double, 5> current{1, 0, 0, 0, 0}, target{1, 0, 0, 0, 0};
        std::array<double, 16> z1{}, z2{};
        double smoothing = 0.002;
        void step() noexcept;
        float tick(float, int) noexcept;
    };
    IrLoader impulses;
    juce::AudioBuffer<float> wet, dry, original, delay;
    std::array<Filter, 12> filters;
    std::array<juce::SmoothedValue<float>, 7> smooth;
    double rate = 48000;
    int blockSize = 512, channelCount = 2, writePosition = 0;
    float delayOld = 0, delayNew = 0, delayBlend = 1;
    bool initial = true;
};
} // namespace ch
