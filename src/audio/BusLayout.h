#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>
#include <vector>
namespace ch
{
enum class BusInterpretation
{
    followHost,
    discrete,
    ambix
};
struct StereoPair
{
    int left = 0, right = 1;
};
struct BusLayout
{
    juce::AudioChannelSet channels;
    BusInterpretation interpretation = BusInterpretation::followHost;
    juce::StringArray labels;
    std::vector<StereoPair> stereoPairs;
    int lfe = -1;
    static std::optional<BusLayout> fromHost(const juce::AudioChannelSet &, BusInterpretation);
    juce::String identity() const;
    bool isAmbix() const { return channels.getAmbisonicOrder() >= 1; }
};
} // namespace ch
