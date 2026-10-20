#pragma once
#include "audio/ReverbProcessor.h"
#include "state/ToneState.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace ch
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
struct ParameterRefs
{
    explicit ParameterRefs(juce::AudioProcessorValueTreeState &);
    ParameterSnapshot read() const noexcept;
    DampingSettings readDamping() const noexcept;
    std::array<std::atomic<float> *, 19> values{};
    std::array<std::atomic<float> *, 10> bands{};
    std::array<std::array<std::atomic<float> *, 4>, 10> shapes{};
    std::array<std::array<std::atomic<float> *, 2>, 6> damping{};
    std::atomic<float> *eqBypass = nullptr, *dampProtect = nullptr, *dampBypass = nullptr;
};
} // namespace ch
