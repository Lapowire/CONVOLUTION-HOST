#include "RealtimeGuard.h"
#include "TestSupport.h"
#include "audio/ParametricEq.h"
TEST(parametric_eq_analytic_and_realtime_response)
{
    for (double sr : {44100., 48000., 96000.})
    {
        ch::EqSettings s;
        s.bands[5].frequencyHz = 1000;
        s.gainDb[5] = 12;
        test::near(float(ch::ParametricEq::magnitude(s, sr, 1000)), std::pow(10.f, 12.f / 20), 1e-3f);
        ch::ParametricEq eq;
        eq.prepare(sr, 2);
        eq.setTargets(s);
        juce::AudioBuffer<float> b(2, 256);
        double input = 0, output = 0;
        int pos = 0;
        for (int block = 0; block < 100; ++block)
        {
            b.clear();
            for (int i = 0; i < 256; ++i)
                b.setSample(0, i, float(std::sin(2 * juce::MathConstants<double>::pi * 1000 * pos++ / sr)));
            if (block > 80)
                for (int i = 0; i < 256; ++i)
                    input += double(b.getSample(0, i)) * b.getSample(0, i);
            {
                test::RealtimeGuard guard;
                eq.process(b);
            }
            REQUIRE(test::realtimeAllocations == 0);
            for (int i = 0; i < 256; ++i)
            {
                REQUIRE(b.getSample(1, i) == 0);
                if (block > 80)
                    output += double(b.getSample(0, i)) * b.getSample(0, i);
            }
        }
        test::near(float(10 * std::log10(output / input)), 12, 0.15f);
        s.bands[5].type = ch::EqType::notch;
        REQUIRE(ch::ParametricEq::magnitude(s, sr, 1000) < 1e-5);
        s.bypass = true;
        test::near(float(ch::ParametricEq::magnitude(s, sr, 1000)), 1);
    }
}
TEST(parametric_eq_extreme_automation_is_finite_and_allocation_free)
{
    for (double sr : {8000., 44100., 96000.})
    {
        ch::ParametricEq eq;
        eq.prepare(sr, 16);
        ch::EqSettings s;
        juce::AudioBuffer<float> b(16, 1024);
        for (int k = 0; k < 300; ++k)
        {
            const int frames = std::array<int, 5>{1, 37, 64, 256, 1024}[std::size_t(k % 5)];
            b.setSize(16, frames, false, false, true);
            for (int i = 0; i < 10; ++i)
            {
                s.bands[i].frequencyHz = k % 2 ? 20 : 20000;
                s.bands[i].q = k % 2 ? 0.1f : 18;
                s.bands[i].type = ch::EqType(k % 4);
                s.gainDb[i] = k % 2 ? 12 : -12;
                s.bands[i].enabled = k % 5 != 0;
            }
            s.bypass = k % 7 == 0;
            b.clear();
            b.setSample(0, 0, 0.01f);
            {
                test::RealtimeGuard guard;
                eq.setTargets(s);
                eq.process(b);
            }
            REQUIRE(test::realtimeAllocations == 0);
            for (int c = 0; c < 16; ++c)
                for (int i = 0; i < frames; ++i)
                    REQUIRE(std::isfinite(b.getSample(c, i)));
        }
    }
}
#include <chrono>
#include <juce_dsp/juce_dsp.h>
TEST(parametric_eq_preserves_legacy_reference_and_shelf_endpoints)
{
    for (double sr : {44100., 48000., 96000.})
    {
        ch::EqSettings settings;
        for (std::size_t i = 0; i < 10; ++i)
            settings.gainDb[i] = i % 2 ? 6 : -4;
        for (double f : {40., 200., 1000., 5000., 16000.})
        {
            double reference = 1;
            for (std::size_t i = 0; i < 10; ++i)
                reference *= juce::dsp::IIR::Coefficients<double>::makePeakFilter(
                                 sr, ch::eqFrequencies[i], 1.4, std::pow(10., settings.gainDb[i] / 20.))
                                 ->getMagnitudeForFrequency(f, sr);
            test::near(float(ch::ParametricEq::magnitude(settings, sr, f)), float(reference), 1e-4f);
        }
        settings = {};
        settings.gainDb[5] = 6;
        settings.bands[5].type = ch::EqType::lowShelf;
        test::near(float(ch::ParametricEq::magnitude(settings, sr, 1)), std::pow(10.f, 0.3f), 0.001f);
        settings.bands[5].type = ch::EqType::highShelf;
        test::near(float(ch::ParametricEq::magnitude(settings, sr, 1)), 1, 0.001f);
        test::near(float(ch::ParametricEq::magnitude(settings, sr, sr * 0.49)), std::pow(10.f, 0.3f), 0.001f);
    }
}
TEST(parametric_eq_automation_callback_measurement)
{
    ch::ParametricEq eq;
    eq.prepare(48000, 16);
    ch::EqSettings s;
    juce::AudioBuffer<float> b(16, 256);
    double worst = 0, total = 0;
    for (int block = 0; block < 101; ++block)
    {
        s.bands[5].frequencyHz = block % 2 ? 500 : 4000;
        s.bands[5].q = block % 2 ? 0.5f : 8;
        s.gainDb[5] = block % 2 ? 6 : -6;
        eq.setTargets(s);
        for (int c = 0; c < 16; ++c)
            for (int i = 0; i < 256; ++i)
                b.setSample(c, i, float(std::sin((block * 256 + i) * 0.1) * 0.01));
        auto start = std::chrono::steady_clock::now();
        {
            test::RealtimeGuard guard;
            eq.process(b);
        }
        REQUIRE(test::realtimeAllocations == 0);
        double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        worst = std::max(worst, ms);
        total += ms;
    }
    std::cout << "MEASURE parametric EQ automated 16ch/48kHz/101x256: " << total << " ms total; " << worst
              << " ms worst callback (5.333 ms audio block)\n";
}
