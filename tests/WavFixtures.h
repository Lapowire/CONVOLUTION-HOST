#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <stdexcept>
namespace test
{
struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                         .getChildFile("ch-test-" + juce::Uuid().toString());
    TempDir()
    {
        if (!dir.createDirectory())
            throw std::runtime_error("temp dir failed");
    }
    ~TempDir() { dir.deleteRecursively(); }
    juce::File wav(const juce::String &name, const juce::AudioBuffer<float> &data, double rate = 48000,
                   bool floating = true)
    {
        auto f = dir.getChildFile(name);
        juce::WavAudioFormat fmt;
        auto out = f.createOutputStream();
        if (!out)
            throw std::runtime_error("output failed");
        std::unique_ptr<juce::AudioFormatWriter> w(fmt.createWriterFor(
            out.release(), rate, static_cast<unsigned>(data.getNumChannels()), floating ? 32 : 16, {}, 0));
        if (!w || !w->writeFromAudioSampleBuffer(data, 0, data.getNumSamples()))
            throw std::runtime_error("WAV fixture failed");
        return f;
    }
};
} // namespace test
