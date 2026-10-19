#pragma once
#include "audio/ReverbProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace ch {
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
struct ParameterRefs {
 explicit ParameterRefs(juce::AudioProcessorValueTreeState&);
 ParameterSnapshot read() const noexcept;
 std::array<std::atomic<float>*,19> values{};
 std::array<std::atomic<float>*,10> bands{};
};
}
