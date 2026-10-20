#pragma once
#include "audio/ParametricEq.h"
#include "ir/SpectralDamping.h"
#include <juce_data_structures/juce_data_structures.h>
#include <optional>
#include <vector>
namespace ch
{
struct ToneParameter
{
    juce::String id,name;
    float minimum,maximum,defaultValue,interval;
    bool logarithmic=false;
};
// Includes existing gain parameters; appendedToneParameters excludes them.
std::vector<ToneParameter> toneParameterDefinitions(bool appendedOnly=false);
struct ToneState
{
    EqSettings eq;
    DampingSettings damping;
    juce::ValueTree parametersTree() const;
    static std::optional<ToneState> fromParameters(const juce::ValueTree&,bool requireAll=false);
    bool operator==(const ToneState&) const = default;
};
struct ToneMemories
{
    std::array<ToneState,2> slots;
    int active=0;
    juce::ValueTree encode() const;
    static std::optional<ToneMemories> decode(const juce::ValueTree&);
};
}
