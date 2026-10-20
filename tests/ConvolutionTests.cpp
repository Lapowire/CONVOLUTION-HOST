#include "TestSupport.h"
#include "audio/ConvolutionEngine.h"
#include <random>
TEST(convolution_matches_reference_all_routes_and_blocks)
{
    for (double sr : {44100., 48000., 96000.})
        for (int block : {64, 256, 1024})
            for (int channels : {1, 2})
            {
                auto ir = std::make_shared<ch::PreparedIr>();
                ir->sampleRate = sr;
                const int irChannels = channels == 1 ? 1 : 4;
                ir->samples.setSize(irChannels, 513);
                ir->samples.clear();
                for (int c = 0; c < irChannels; ++c)
                    for (int i = 0; i < 513; ++i)
                        ir->samples.setSample(
                            c, i, float((c + 1) * 0.01 * std::exp(-i / 80.) * std::cos(i * 0.11)));
                auto routing =
                    channels == 1
                        ? ch::RoutingModel::diagonal(1, 1)
                        : ch::RoutingModel{{{0, 0, 0, 1}, {0, 1, 1, 1}, {1, 0, 2, 1}, {1, 1, 3, 1}}};
                ch::ConvolutionEngine engine(ir, routing);
                engine.prepare(sr, block, channels);
                constexpr int length = 2200;
                juce::AudioBuffer<float> source(channels, length), actual(channels, length);
                source.clear();
                actual.clear();
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 900; ++i)
                        source.setSample(c, i, float(std::sin(i * 0.23 + c) * 0.2));
                int offset = 0;
                while (offset < length)
                {
                    const int n = std::min(length - offset, offset % 3 == 0 ? block : std::max(1, block / 3));
                    juce::AudioBuffer<float> b(channels, n);
                    for (int c = 0; c < channels; ++c)
                        b.copyFrom(c, 0, source, c, offset, n);
                    engine.process(b);
                    for (int c = 0; c < channels; ++c)
                        actual.copyFrom(c, offset, b, c, 0, n);
                    offset += n;
                }
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < length; ++i)
                    {
                        float expected = 0;
                        for (auto r : routing.routes)
                            if (r.output == c)
                                for (int k = 0; k <= std::min(i, 512); ++k)
                                    expected += source.getSample(r.input, i - k) *
                                                ir->samples.getSample(r.irChannel, k) * r.gain;
                        test::near(actual.getSample(c, i), expected);
                    }
                REQUIRE(engine.latencySamples() == 0);
                engine.reset();
                juce::AudioBuffer<float> silence(channels, block);
                silence.clear();
                engine.process(silence);
                test::near(silence.getMagnitude(0, block), 0);
            }
}
TEST(diagonal_has_no_crossfeed)
{
    auto ir = std::make_shared<ch::PreparedIr>();
    ir->samples.setSize(2, 1);
    ir->samples.setSample(0, 0, 0.25f);
    ir->samples.setSample(1, 0, 0.5f);
    ch::ConvolutionEngine engine(ir, ch::RoutingModel::diagonal(2, 2));
    engine.prepare(48000, 64, 2);
    juce::AudioBuffer<float> b(2, 64);
    b.clear();
    b.setSample(0, 0, 1);
    engine.process(b);
    test::near(b.getSample(0, 0), 0.25f);
    test::near(b.getMagnitude(1, 0, 64), 0);
}
#include "RealtimeGuard.h"
TEST(convolution_does_not_allocate_or_destroy_on_audio_thread)
{
    auto ir = std::make_shared<ch::PreparedIr>();
    ir->samples.setSize(2, 8192);
    ir->samples.clear();
    ir->samples.setSample(0, 0, 1);
    ir->samples.setSample(1, 0, 1);
    ch::ConvolutionEngine engine(ir, ch::RoutingModel::diagonal(2, 2));
    engine.prepare(48000, 256, 2);
    juce::AudioBuffer<float> b(2, 256);
    b.clear();
    {
        test::RealtimeGuard guard;
        for (int n = 0; n < 100; ++n)
            engine.process(b);
    }
    REQUIRE(test::realtimeAllocations == 0);
}
TEST(large_host_blocks_do_not_allocate_fft_scratch_on_audio_thread)
{
    auto ir = std::make_shared<ch::PreparedIr>();
    ir->samples.setSize(1, 64);
    ir->samples.clear();
    ir->samples.setSample(0, 0, 0.5f);
    ch::ConvolutionEngine engine(ir, ch::RoutingModel::diagonal(1, 1));
    engine.prepare(48000, 65536, 1);
    juce::AudioBuffer<float> audio(1, 65536);
    audio.clear();
    audio.setSample(0, 0, 1);
    {
        test::RealtimeGuard guard;
        engine.process(audio);
    }
    REQUIRE(test::realtimeAllocations == 0);
    test::near(audio.getSample(0, 0), 0.5f);
}
TEST(multichannel_sparse_matrix_matches_reference)
{
    for (int channels : {4, 6, 8, 9, 10, 12, 16})
        for (double sr : {44100., 48000., 96000.})
            for (int block : {64, 256, 1024})
            {
                auto ir = std::make_shared<ch::PreparedIr>();
                ir->sampleRate = sr;
                ir->samples.setSize(channels, 17);
                ir->samples.clear();
                ch::RoutingModel routes(std::vector<ch::Route>{});
                for (int c = 0; c < channels; ++c)
                {
                    ir->samples.setSample(c, c % 17, float(c + 1) / 32);
                    routes.routes.push_back({c, (c + 3) % channels, c, c % 2 ? -0.5f : 0.5f});
                }
                ch::ConvolutionEngine engine(ir, routes);
                engine.prepare(sr, block, channels);
                juce::AudioBuffer<float> audio(channels, 37);
                audio.clear();
                for (int c = 0; c < channels; ++c)
                    audio.setSample(c, 0, 0.25f);
                {
                    test::RealtimeGuard guard;
                    engine.process(audio);
                }
                REQUIRE(test::realtimeAllocations == 0);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < 37; ++i)
                    {
                        int input = (c - 3 + channels) % channels;
                        test::near(
                            audio.getSample(c, i),
                            i == input % 17 ? 0.25f * float(input + 1) / 32 * (input % 2 ? -0.5f : 0.5f) : 0,
                            2e-4f);
                    }
            }
}
#include <chrono>
TEST(sixteen_channel_resources_and_overbudget_matrix)
{
    auto ir = std::make_shared<ch::PreparedIr>();
    ir->samples.setSize(1, 96000);
    ir->samples.clear();
    ir->samples.setSample(0, 0, 0.5f);
    ch::ConvolutionEngine engine(ir, ch::RoutingModel::diagonal(16, 1));
    engine.prepare(48000, 256, 16);
    juce::AudioBuffer<float> audio(16, 256);
    audio.clear();
    auto start = std::chrono::steady_clock::now();
    {
        test::RealtimeGuard guard;
        for (int i = 0; i < 101; ++i)
            engine.process(audio);
    }
    REQUIRE(test::realtimeAllocations == 0);
    std::cout << "MEASURE 16ch/2s/48kHz 101x256: "
              << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count()
              << " ms\n";
    ch::RoutingModel dense(std::vector<ch::Route>{});
    for (int i = 0; i < 16; ++i)
        for (int o = 0; o < 16; ++o)
            dense.routes.push_back({i, o, 0, 1});
    bool rejected = false;
    try
    {
        ch::ConvolutionEngine tooLarge(ir, dense);
        tooLarge.prepare(48000, 256, 16);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    REQUIRE(rejected);
}
