#pragma once
#include "ir/PreparedIr.h"
#include "audio/RoutingModel.h"
#include <juce_data_structures/juce_data_structures.h>
namespace ch {
struct SavedState {juce::ValueTree parameters;IrRequest request;RoutingModel routing;juce::String hash,relativePath;};
class StateStore {
public:
 static juce::ValueTree encode(const SavedState&);
 static bool decode(const juce::ValueTree&,SavedState&);
 static juce::File resolve(const SavedState&,const juce::StringArray& roots);
};
}
