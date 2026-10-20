#pragma once
#include <juce_core/juce_core.h>
namespace ch
{
struct PresetFile
{
    static juce::Result save(const juce::File &, const juce::MemoryBlock &);
    static juce::Result load(const juce::File &, juce::MemoryBlock &);
};
} // namespace ch
