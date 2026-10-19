#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
namespace ch
{
struct IrRequest
{
    juce::File file;
    double sampleRate = 48000;
    double startMs = 0, endMs = 0, fadeInMs = 0, fadeOutMs = 0;
    bool normalise = false;
    juce::String expectedHash, relativePath;
    juce::StringArray searchRoots;
    std::vector<juce::File> additionalFiles;
    juce::StringArray sourceHashes, additionalRelativePaths;
};
struct PreparedIr
{
    juce::AudioBuffer<float> samples;
    double sampleRate = 48000, originalSampleRate = 48000;
    juce::File file;
    juce::String hash;
    juce::StringArray sourceHashes;
    std::vector<float> waveform, decayDb;
    std::vector<std::vector<float>> channelWaveforms;
    std::size_t estimatedBytes = 0;
    double durationSeconds() const { return samples.getNumSamples() / sampleRate; }
};
struct DecodeResult
{
    std::shared_ptr<const PreparedIr> ir;
    juce::String error;
    explicit operator bool() const { return ir != nullptr; }
};
} // namespace ch
