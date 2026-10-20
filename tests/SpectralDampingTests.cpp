#include "TestSupport.h"
#include "ir/SpectralDamping.h"
#include <cstring>
TEST(neutral_damping_preserves_every_sample)
{
    ch::DampingSettings d;
    juce::AudioBuffer<float> b(2, 37);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 37; ++i)
            b.setSample(c, i, float(std::sin(i + c)));
    auto copy = b;
    REQUIRE(ch::SpectralDamping::apply(b, 48000, d, {}).wasOk());
    for (int c = 0; c < 2; ++c)
        REQUIRE(std::memcmp(b.getReadPointer(c), copy.getReadPointer(c), 37 * sizeof(float)) == 0);
    d.nodes[1].frequencyHz = d.nodes[0].frequencyHz;
    REQUIRE(!d.isValid());
    REQUIRE(ch::SpectralDamping::apply(b, 48000, d, {}).failed());
}
TEST(damping_changes_decay_by_frequency_and_preserves_channels)
{
    for (double sr : {44100., 48000., 96000.})
    {
        ch::DampingSettings d;
        d.protectMs = 0;
        for (auto &node : d.nodes)
            node.attenuationDbPerSecond = 12;
        const int n = int(sr);
        juce::AudioBuffer<float> b(2, n);
        for (int i = 0; i < n; ++i)
        {
            float x = float(std::sin(2 * juce::MathConstants<double>::pi * 1000 * i / sr));
            b.setSample(0, i, x);
            b.setSample(1, i, -0.5f * x);
        }
        REQUIRE(ch::SpectralDamping::apply(b, sr, d, {}).wasOk());
        for (double t : {0.25, 0.75})
        {
            double sum = 0, ref = 0;
            for (int i = int((t - 0.01) * sr); i < int((t + 0.01) * sr); ++i)
            {
                sum += double(b.getSample(0, i)) * b.getSample(0, i);
                ref += std::pow(std::sin(2 * juce::MathConstants<double>::pi * 1000 * i / sr), 2);
            }
            test::near(float(10 * std::log10(sum / ref)), float(-12 * t), 0.2f);
        }
        for (int i = 0; i < n; ++i)
            test::near(b.getSample(1, i), -0.5f * b.getSample(0, i), 2e-4f);
        REQUIRE(ch::SpectralDamping::scratchBytes(sr) < 16 * 1024 * 1024);
    }
}
TEST(damping_reconstructs_short_edges_and_rejects_cancelled_jobs)
{
    for (int n : {1, 37, 1024, 5000})
    {
        juce::AudioBuffer<float> b(1, n);
        b.clear();
        b.setSample(0, 0, 0.5f);
        b.setSample(0, n - 1, 0.5f);
        ch::DampingSettings d;
        for (auto &node : d.nodes)
            node.attenuationDbPerSecond = 1e-5f;
        REQUIRE(ch::SpectralDamping::apply(b, 48000, d, {}).wasOk());
        test::near(b.getSample(0, 0), 0.5f, 2e-4f);
        test::near(b.getSample(0, n - 1), 0.5f, 2e-4f);
        for (int i = 1; i < n - 1; ++i)
            test::near(b.getSample(0, i), 0, 2e-4f);
        REQUIRE(ch::SpectralDamping::apply(b, 48000, d, [] { return true; }).failed());
    }
}
TEST(damping_is_frequency_selective)
{
    ch::DampingSettings d;
    d.protectMs = 0;
    for (std::size_t i = 3; i < 6; ++i)
        d.nodes[i].attenuationDbPerSecond = 24;
    juce::AudioBuffer<float> b(2, 48000);
    for (int i = 0; i < 48000; ++i)
    {
        b.setSample(0, i, float(std::sin(2 * juce::MathConstants<double>::pi * 100 * i / 48000)));
        b.setSample(1, i, float(std::sin(2 * juce::MathConstants<double>::pi * 8000 * i / 48000)));
    }
    REQUIRE(ch::SpectralDamping::apply(b, 48000, d, {}).wasOk());
    for (int c = 0; c < 2; ++c)
    {
        double energy = 0;
        for (int i = 35520; i < 36480; ++i)
            energy += double(b.getSample(c, i)) * b.getSample(c, i);
        test::near(float(10 * std::log10(energy / 480)), c == 0 ? 0.f : -18.f, 0.3f);
    }
}
#include <chrono>
TEST(damping_long_ir_resource_measurement)
{
    for (auto configuration : {std::array<int, 3>{96000, 30, 2}, {48000, 2, 16}})
    {
        int sr = configuration[0], seconds = configuration[1], channels = configuration[2];
        juce::AudioBuffer<float> buffer(channels, sr * seconds);
        buffer.clear();
        for (int c = 0; c < channels; ++c)
        {
            buffer.setSample(c, 0, 0.5f);
            buffer.setSample(c, sr * seconds - 1, 0.1f);
        }
        ch::DampingSettings settings;
        settings.nodes[5].attenuationDbPerSecond = 24;
        auto start = std::chrono::steady_clock::now();
        REQUIRE(ch::SpectralDamping::apply(buffer, sr, settings, {}).wasOk());
        double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "MEASURE damping " << channels << "ch/" << seconds << "s/" << sr << "Hz: " << ms
                  << " ms preparation; " << ch::SpectralDamping::scratchBytes(sr)
                  << " bytes conservative scratch\n";
        REQUIRE(buffer.getNumSamples() == sr * seconds);
        REQUIRE(std::isfinite(buffer.getSample(0, 0)));
    }
}
