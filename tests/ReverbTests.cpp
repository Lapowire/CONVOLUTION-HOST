#include "RealtimeGuard.h"
#include "TestSupport.h"
#include "WavFixtures.h"
#include "audio/ReverbProcessor.h"
#include <chrono>
static void initReverb(ch::ReverbProcessor &p, test::TempDir &tmp, int channels = 2, double sr = 48000)
{
    p.prepare(sr, 256, channels);
    juce::AudioBuffer<float> ir(1, 1);
    ir.setSample(0, 0, 1);
    p.loader().request({tmp.wav("unit.wav", ir, sr)});
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (p.loader().status().loading && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(p.loader().status().error.isEmpty());
    REQUIRE(!p.loader().status().loading);
}
TEST(no_ir_is_unity_and_mix_gain_width_are_correct)
{
    ch::ReverbProcessor empty;
    empty.prepare(48000, 256, 2);
    ch::ParameterSnapshot params;
    params.inputGain = 12;
    params.outputGain = 12;
    juce::AudioBuffer<float> b(2, 64);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 64; ++i)
            b.setSample(c, i, 0.125f);
    empty.process(b, params);
    test::near(b.getSample(0, 0), 0.125f);
    for (float width : {0.f, 100.f, 200.f})
    {
        test::TempDir tmp;
        ch::ReverbProcessor p;
        initReverb(p, tmp);
        params = {};
        params.mix = 100;
        params.width = width;
        b.clear();
        b.setSample(0, 0, 1);
        p.process(b, params);
        test::near(b.getSample(0, 0), 0.5f + width / 200);
        test::near(b.getSample(1, 0), 0.5f - width / 200);
    }
    for (float mix : {0.f, 25.f, 100.f})
    {
        test::TempDir tmp;
        ch::ReverbProcessor p;
        initReverb(p, tmp);
        params = {};
        params.mix = mix;
        params.inputGain = -6.0206f;
        params.outputGain = -6.0206f;
        b.clear();
        b.setSample(0, 0, 1);
        p.process(b, params);
        test::near(b.getSample(0, 0), 0.25f);
    }
}
TEST(predelay_and_bypass)
{
    for (bool bypass : {false, true})
    {
        test::TempDir tmp;
        ch::ReverbProcessor p;
        initReverb(p, tmp, 1);
        ch::ParameterSnapshot params;
        params.mix = 100;
        params.predelay = 1;
        params.bypass = bypass;
        juce::AudioBuffer<float> b(1, 128);
        b.clear();
        b.setSample(0, 0, 1);
        p.process(b, params);
        test::near(b.getSample(0, bypass ? 0 : 48), 1);
        test::near(b.getSample(0, bypass ? 48 : 0), 0);
    }
}
TEST(eq_centres_have_expected_gain_and_no_audio_allocations)
{
    for (int band = 0; band < 10; ++band)
    {
        test::TempDir tmp;
        ch::ReverbProcessor p;
        initReverb(p, tmp, 1);
        ch::ParameterSnapshot params;
        params.mix = 100;
        params.eq[std::size_t(band)] = 6;
        juce::AudioBuffer<float> b(1, 256);
        double inputEnergy = 0, outputEnergy = 0;
        int position = 0;
        for (int block = 0; block < 200; ++block)
        {
            for (int i = 0; i < 256; ++i)
            {
                float v = 0.1f * std::sin(float(2 * juce::MathConstants<double>::pi *
                                                ch::eqFrequencies[std::size_t(band)] * (position++) / 48000));
                b.setSample(0, i, v);
                if (block > 100)
                    inputEnergy += v * v;
            }
            {
                test::RealtimeGuard guard;
                p.process(b, params);
            }
            REQUIRE(test::realtimeAllocations == 0);
            if (block > 100)
                for (int i = 0; i < 256; ++i)
                    outputEnergy += std::pow(b.getSample(0, i), 2);
        }
        test::near(float(10 * std::log10(outputEnergy / inputEnergy)), 6, 0.15f);
    }
}
TEST(automation_and_low_sample_rate_remain_finite)
{
    test::TempDir tmp;
    ch::ReverbProcessor p;
    initReverb(p, tmp, 2, 8000);
    ch::ParameterSnapshot params;
    params.mix = 100;
    params.filtersEnabled = true;
    params.highpass = 2000;
    params.lowpass = 20000;
    params.eq.fill(12);
    juce::AudioBuffer<float> b(2, 256);
    float previous = 0;
    for (int block = 0; block < 80; ++block)
    {
        params.predelay = block % 2 ? 500 : 0;
        params.bypass = block % 3 == 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                b.setSample(c, i, 0.1f);
        {
            test::RealtimeGuard guard;
            p.process(b, params);
        }
        REQUIRE(test::realtimeAllocations == 0);
        for (int i = 0; i < 256; ++i)
        {
            float v = b.getSample(0, i);
            REQUIRE(std::isfinite(v));
            REQUIRE(std::abs(v - previous) < 1);
            previous = v;
        }
    }
}
TEST(wet_filters_attenuate_extreme_frequencies)
{
    for (bool highpass : {false, true})
    {
        test::TempDir tmp;
        ch::ReverbProcessor p;
        initReverb(p, tmp, 1);
        ch::ParameterSnapshot params;
        params.mix = 100;
        params.filtersEnabled = true;
        params.highpass = highpass ? 1000 : 20;
        params.lowpass = highpass ? 20000 : 1000;
        const double freq = highpass ? 100 : 10000;
        double energy = 0;
        int pos = 0;
        juce::AudioBuffer<float> b(1, 256);
        for (int k = 0; k < 80; ++k)
        {
            for (int i = 0; i < 256; ++i)
                b.setSample(
                    0, i, float(0.1 * std::sin(2 * juce::MathConstants<double>::pi * freq * pos++ / 48000)));
            p.process(b, params);
            if (k > 40)
                for (int i = 0; i < 256; ++i)
                    energy += std::pow(b.getSample(0, i), 2);
        }
        REQUIRE(std::sqrt(energy / (39 * 256)) < 0.002);
    }
}
TEST(long_ir_resource_measurement)
{
    test::TempDir tmp;
    ch::ReverbProcessor p;
    p.prepare(96000, 256, 2);
    juce::AudioBuffer<float> impulse(1, 96000 * 30);
    impulse.clear();
    impulse.setSample(0, 0, 1);
    impulse.setSample(0, impulse.getNumSamples() - 1, 0.001f);
    auto f = tmp.wav("long-valid.wav", impulse, 96000);
    p.loader().request({f});
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (p.loader().status().loading && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    REQUIRE(!p.loader().status().loading);
    REQUIRE(p.loader().status().error.isEmpty());
    ch::ParameterSnapshot params;
    params.mix = 100;
    juce::AudioBuffer<float> b(2, 256);
    b.clear();
    b.setSample(0, 0, 1);
    const auto start = std::chrono::steady_clock::now();
    p.process(b, params);
    test::near(b.getSample(0, 0), 1);
    for (int i = 0; i < 100; ++i)
    {
        b.clear();
        p.process(b, params);
    }
    const auto ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "MEASURE 30s/96kHz stereo, 101x256 blocks: " << ms << " ms; conservative two-engine bound "
              << ch::IrDecoder::estimateBytes(2880000, 2, 256) / 1024 / 1024 << " MiB\n";
}
TEST(rapid_predelay_retargeting_keeps_sine_continuous)
{
    test::TempDir tmp;
    ch::ReverbProcessor p;
    initReverb(p, tmp, 1);
    ch::ParameterSnapshot params;
    params.mix = 100;
    juce::AudioBuffer<float> audio(1, 64);
    int position = 0;
    float previous = 0, maxJump = 0;
    for (int block = 0; block < 800; ++block)
    {
        if (block >= 400)
            params.predelay = block % 2 ? 13 : 0;
        for (int i = 0; i < 64; ++i)
            audio.setSample(0, i,
                            float(std::sin(2 * juce::MathConstants<double>::pi * 100 * position++ / 48000)));
        p.process(audio, params);
        if (block >= 400)
            maxJump = std::max(maxJump, std::abs(audio.getSample(0, 0) - previous));
        previous = audio.getSample(0, 63);
    }
    REQUIRE(maxJump < 0.05f);
}
TEST(width_only_changes_designated_nonoverlapping_pairs)
{
    test::TempDir tmp;
    ch::ReverbProcessor p;
    initReverb(p, tmp, 4);
    std::array<ch::StereoPair, 1> pair{{{0, 2}}};
    p.configureWidthPairs(pair);
    ch::ParameterSnapshot params;
    params.mix = 100;
    params.width = 0;
    juce::AudioBuffer<float> audio(4, 256);
    audio.clear();
    audio.setSample(0, 0, 1);
    audio.setSample(1, 0, 0.25f);
    p.process(audio, params);
    test::near(audio.getSample(0, 0), 0.5f);
    test::near(audio.getSample(2, 0), 0.5f);
    test::near(audio.getSample(1, 0), 0.25f);
    test::near(audio.getSample(3, 0), 0);
    bool rejected = false;
    try
    {
        std::array<ch::StereoPair, 2> bad{{{0, 1}, {1, 2}}};
        p.configureWidthPairs(bad);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    REQUIRE(rejected);
    rejected = false;
    try
    {
        std::array<ch::StereoPair, 1> bad{{{0, 4}}};
        p.configureWidthPairs(bad);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    REQUIRE(rejected);
}
