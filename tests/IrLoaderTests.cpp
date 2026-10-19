#include "TestSupport.h"
#include "WavFixtures.h"
#include "ir/IrLoader.h"
#include <chrono>
static bool awaitReady(ch::IrLoader &loader)
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < until)
    {
        if (!loader.status().loading)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}
TEST(loader_activates_latest_and_keeps_previous_on_error)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> impulse(1, 1);
    impulse.setSample(0, 0, 0.25f);
    auto a = tmp.wav("a.wav", impulse);
    impulse.setSample(0, 0, 0.75f);
    auto b = tmp.wav("b.wav", impulse);
    ch::IrLoader loader;
    loader.configure(48000, 64, 1);
    loader.request({a});
    auto id = loader.request({b});
    REQUIRE(id > 0);
    REQUIRE(awaitReady(loader));
    juce::AudioBuffer<float> audio(1, 64);
    audio.clear();
    audio.setSample(0, 0, 1);
    loader.processWet(audio);
    REQUIRE(loader.hasActive());
    REQUIRE(loader.activeRequest() == id);
    test::near(audio.getSample(0, 0), 0.75f);
    loader.request({tmp.dir.getChildFile("missing.wav")});
    REQUIRE(awaitReady(loader));
    REQUIRE(loader.status().error.isNotEmpty());
    audio.clear();
    audio.setSample(0, 0, 1);
    loader.processWet(audio);
    test::near(audio.getSample(0, 0), 0.75f);
}
TEST(loader_crossfade_and_configuration_generation)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> impulse(1, 1);
    impulse.setSample(0, 0, 0.25f);
    auto a = tmp.wav("a.wav", impulse);
    impulse.setSample(0, 0, 0.75f);
    auto b = tmp.wav("b.wav", impulse);
    ch::IrLoader loader;
    loader.configure(48000, 64, 1);
    loader.request({a});
    REQUIRE(awaitReady(loader));
    juce::AudioBuffer<float> audio(1, 64);
    for (int i = 0; i < 64; ++i)
        audio.setSample(0, i, 1);
    loader.processWet(audio);
    REQUIRE(loader.hasActive());
    loader.request({b});
    REQUIRE(awaitReady(loader));
    float previous = 0.25f;
    for (int block = 0; block < 42; ++block)
    {
        for (int i = 0; i < 64; ++i)
            audio.setSample(0, i, 1);
        loader.processWet(audio);
        for (int i = 0; i < 64; ++i)
        {
            float v = audio.getSample(0, i);
            REQUIRE(v >= previous - 0.0001f);
            REQUIRE(v <= 0.7501f);
            REQUIRE(v - previous < 0.001f);
            previous = v;
        }
    }
    test::near(previous, 0.75f);
    loader.request({a});
    loader.configure(96000, 64, 1);
    REQUIRE(!loader.hasActive());
    ch::IrRequest request{b};
    request.sampleRate = 96000;
    auto id = loader.request(request);
    REQUIRE(awaitReady(loader));
    audio.clear();
    audio.setSample(0, 0, 1);
    loader.processWet(audio);
    REQUIRE(loader.activeRequest() == id);
    REQUIRE(loader.status().ir->sampleRate == 96000);
}
TEST(loader_supersedes_pending_ir_while_host_is_stopped)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> impulse(1, 1);
    impulse.setSample(0, 0, 0.5f);
    auto f = tmp.wav("ir.wav", impulse);
    ch::IrLoader loader;
    loader.configure(48000, 64, 1);
    loader.request({f});
    REQUIRE(awaitReady(loader));
    juce::AudioBuffer<float> audio(1, 64);
    audio.clear();
    loader.processWet(audio);
    loader.request({f});
    REQUIRE(awaitReady(loader));
    auto id = loader.request({f});
    REQUIRE(awaitReady(loader));
    audio.clear();
    loader.processWet(audio);
    REQUIRE(loader.activeRequest() == id);
}
#include "RealtimeGuard.h"
TEST(loader_swaps_without_audio_thread_allocation)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> impulse(1, 64);
    impulse.clear();
    impulse.setSample(0, 0, 0.5f);
    auto f = tmp.wav("ir.wav", impulse);
    ch::IrLoader loader;
    loader.configure(48000, 64, 1);
    loader.request({f});
    REQUIRE(awaitReady(loader));
    juce::AudioBuffer<float> b(1, 64);
    b.clear();
    {
        test::RealtimeGuard guard;
        loader.processWet(b);
    }
    REQUIRE(test::realtimeAllocations == 0);
    loader.request({f});
    REQUIRE(awaitReady(loader));
    {
        test::RealtimeGuard guard;
        for (int i = 0; i < 45; ++i)
            loader.processWet(b);
    }
    REQUIRE(test::realtimeAllocations == 0);
}
TEST(loader_relocates_state_by_hash_and_rejects_changed_content)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> b(1, 32);
    b.clear();
    b.setSample(0, 0, 0.5f);
    auto f = tmp.wav("Room.wav", b);
    auto decoded = ch::IrDecoder{}.decode({f});
    REQUIRE(decoded);
    ch::IrRequest request{tmp.dir.getChildFile("old/Room.wav")};
    request.expectedHash = decoded.ir->hash;
    request.relativePath = "Room.wav";
    request.searchRoots.add(tmp.dir.getFullPathName());
    ch::IrLoader loader;
    loader.configure(48000, 64, 1);
    loader.request(request);
    REQUIRE(awaitReady(loader));
    REQUIRE(loader.status().error.isEmpty());
    REQUIRE(loader.status().ir);
    juce::AudioBuffer<float> audio(1, 64);
    audio.clear();
    audio.setSample(0, 0, 1);
    loader.processWet(audio);
    test::near(audio.getSample(0, 0), 0.5f);
    b.setSample(0, 0, 0.75f);
    tmp.wav("Room.wav", b);
    loader.request(request);
    REQUIRE(awaitReady(loader));
    REQUIRE(loader.status().error.isNotEmpty());
}
