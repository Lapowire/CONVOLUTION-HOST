#pragma once
#include "ToneState.h"
#include "audio/RoutingModel.h"
#include "ir/PreparedIr.h"
#include <juce_data_structures/juce_data_structures.h>
#include <optional>
namespace ch
{
struct SavedState
{
    juce::ValueTree parameters;
    ToneMemories toneMemories;
    IrRequest request;
    RoutingModel routing;
    BusInterpretation interpretation = BusInterpretation::followHost;
    juce::String hash, relativePath;
};
class StateStore
{
  public:
    static juce::ValueTree encode(const SavedState &);
    static bool decode(const juce::ValueTree &, SavedState &);
    static std::optional<IrRequest> resolveRequest(const SavedState &, const juce::StringArray &roots);
    static juce::File resolve(const SavedState &, const juce::StringArray &roots);
};
} // namespace ch
