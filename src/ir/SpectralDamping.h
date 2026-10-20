#pragma once
#include <array>
#include <functional>
#include <juce_audio_basics/juce_audio_basics.h>
namespace ch
{
struct DampingNode
{
    float frequencyHz = 1000, attenuationDbPerSecond = 0;
    bool operator==(const DampingNode &) const = default;
};
struct DampingSettings
{
    std::array<DampingNode, 6> nodes{{{63, 0}, {250, 0}, {1000, 0}, {4000, 0}, {8000, 0}, {16000, 0}}};
    float protectMs = 20;
    bool bypass = false;
    bool isValid() const noexcept;
    bool isNeutral() const noexcept;
    float attenuationAt(double frequencyHz) const noexcept;
    bool operator==(const DampingSettings &) const = default;
};
class SpectralDamping
{
  public:
    static juce::Result apply(juce::AudioBuffer<float> &, double, const DampingSettings &,
                              const std::function<bool()> &);
    static std::size_t scratchBytes(double) noexcept;
};
} // namespace ch
