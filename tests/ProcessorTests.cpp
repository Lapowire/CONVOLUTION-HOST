#include "RealtimeGuard.h"
#include "TestSupport.h"
#include "WavFixtures.h"
#include "plugin/PluginProcessor.h"
#include <chrono>
static void setParam(ch::PluginProcessor &p, const char *id, float value)
{
    auto *param = p.parameters.getParameter(id);
    REQUIRE(param);
    param->setValueNotifyingHost(param->convertTo0to1(value));
}
TEST(plugin_parameters_layout_and_state)
{
    test::TempDir tmp;
    ch::PluginProcessor p(tmp.dir.getChildFile("prefs.json"));
    REQUIRE(p.isBusesLayoutSupported(p.getBusesLayout()));
    auto bad = p.getBusesLayout();
    bad.inputBuses.set(0, juce::AudioChannelSet::create5point1());
    bad.outputBuses.set(0, juce::AudioChannelSet::create5point1());
    REQUIRE(!p.isBusesLayoutSupported(bad));
    REQUIRE(p.parameters.getParameter("mix"));
    setParam(p, "mix", 73);
    setParam(p, "eq9", -6);
    setParam(p, "trimStart", 2);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    REQUIRE(state.getSize() > 0);
    ch::PluginProcessor restored(tmp.dir.getChildFile("other.json"));
    restored.setStateInformation(state.getData(), int(state.getSize()));
    test::near(restored.parameters.getRawParameterValue("mix")->load(), 73);
    test::near(restored.parameters.getRawParameterValue("eq9")->load(), -6);
    restored.setStateInformation("invalid", 7);
    test::near(restored.parameters.getRawParameterValue("mix")->load(), 73);
}
TEST(plugin_renders_ir_without_audio_allocation_and_restores_before_prepare)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> ir(1, 128);
    ir.clear();
    ir.setSample(0, 0, 0.5f);
    ir.setSample(0, 127, 0.25f);
    auto file = tmp.wav("Room.wav", ir);
    ch::PluginProcessor p(tmp.dir.getChildFile("prefs.json"));
    setParam(p, "mix", 100);
    p.loadIr(file);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    ch::PluginProcessor restored(tmp.dir.getChildFile("other.json"));
    restored.setStateInformation(state.getData(), int(state.getSize()));
    restored.prepareToPlay(48000, 256);
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (restored.reverb.loader().status().loading && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(restored.reverb.loader().status().error.isEmpty());
    juce::AudioBuffer<float> audio(2, 256);
    audio.clear();
    audio.setSample(0, 0, 1);
    juce::MidiBuffer midi;
    {
        test::RealtimeGuard guard;
        restored.processBlock(audio, midi);
    }
    REQUIRE(test::realtimeAllocations == 0);
    test::near(audio.getSample(0, 0), 0.5f);
    test::near(audio.getSample(0, 127), 0.25f);
    test::near(audio.getMagnitude(1, 0, 256), 0);
    REQUIRE(restored.getLatencySamples() == 0);
    REQUIRE(restored.getTailLengthSeconds() >= 128. / 48000);
}
TEST(plugin_four_mono_files_render_and_relocate_with_session)
{
    test::TempDir tmp;
    std::array<juce::File, 4> files;
    for (int i = 0; i < 4; ++i)
    {
        juce::AudioBuffer<float> ir(1, 1);
        ir.setSample(0, 0, 0.1f * (i + 1));
        files[std::size_t(i)] = tmp.wav(juce::String(i) + ".wav", ir);
    }
    auto prefs = tmp.dir.getChildFile("prefs.json");
    ch::PluginProcessor p(prefs);
    p.prepareToPlay(48000, 256);
    setParam(p, "mix", 100);
    p.loadTrueStereo(files);
    auto wait = [](ch::PluginProcessor &proc)
    {
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (proc.reverb.loader().status().loading && std::chrono::steady_clock::now() < end)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        REQUIRE(proc.reverb.loader().status().ir);
        REQUIRE(proc.reverb.loader().status().error.isEmpty());
    };
    wait(p);
    juce::AudioBuffer<float> audio(2, 256);
    audio.clear();
    audio.setSample(0, 0, 1);
    juce::MidiBuffer midi;
    p.processBlock(audio, midi);
    test::near(audio.getSample(0, 0), 0.1f);
    test::near(audio.getSample(1, 0), 0.2f);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    auto moved = tmp.dir.getChildFile("moved");
    REQUIRE(moved.createDirectory());
    for (auto f : files)
        REQUIRE(f.moveFileTo(moved.getChildFile(f.getFileName())));
    p.library.addRoot(moved, true);
    ch::PluginProcessor restored(prefs);
    restored.setStateInformation(state.getData(), int(state.getSize()));
    restored.prepareToPlay(48000, 256);
    wait(restored);
    audio.clear();
    audio.setSample(1, 0, 1);
    restored.processBlock(audio, midi);
    test::near(audio.getSample(0, 0), 0.3f);
    test::near(audio.getSample(1, 0), 0.4f);
}
static void waitProcessor(ch::PluginProcessor &p)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (p.reverb.loader().status().loading && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(!p.reverb.loader().status().loading);
}
TEST(pending_session_save_keeps_known_hash_and_rejects_replaced_file)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> ir(1, 64);
    ir.clear();
    ir.setSample(0, 0, 0.25f);
    auto file = tmp.wav("known.wav", ir);
    ch::PluginProcessor p(juce::File{});
    p.prepareToPlay(48000, 256);
    p.loadIr(file);
    waitProcessor(p);
    juce::AudioBuffer<float> audio(2, 256);
    audio.clear();
    juce::MidiBuffer midi;
    p.processBlock(audio, midi);
    const auto hash = p.selectedState().hash;
    REQUIRE(hash.length() == 64);
    p.setRouting({});
    waitProcessor(p);
    p.processBlock(audio, midi); // occupy both engines during crossfade
    p.setRouting({});
    REQUIRE(p.reverb.loader().status().loading);
    REQUIRE(p.selectedState().hash == hash);
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    ir.setSample(0, 0, 0.75f);
    tmp.wav("known.wav", ir);
    ch::PluginProcessor restored(juce::File{});
    restored.setStateInformation(saved.getData(), int(saved.getSize()));
    restored.prepareToPlay(48000, 256);
    waitProcessor(restored);
    REQUIRE(restored.reverb.loader().status().error.isNotEmpty());
    REQUIRE(!restored.reverb.loader().status().ir);
}
TEST(failed_four_source_request_keeps_all_content_hashes)
{
    test::TempDir tmp;
    std::array<juce::File, 4> files;
    juce::AudioBuffer<float> ir(1, 64);
    ir.clear();
    ir.setSample(0, 0, 0.25f);
    for (int i = 0; i < 4; ++i)
        files[std::size_t(i)] = tmp.wav(juce::String(i) + ".wav", ir);
    ch::PluginProcessor p(juce::File{});
    p.prepareToPlay(48000, 256);
    p.loadTrueStereo(files);
    waitProcessor(p);
    const auto known = p.selectedState();
    REQUIRE(known.hash.length() == 64);
    REQUIRE(known.request.sourceHashes.size() == 4);
    p.setRouting({{{0, 0, 9, 1}}});
    waitProcessor(p);
    REQUIRE(p.reverb.loader().status().error.isNotEmpty());
    REQUIRE(p.selectedState().hash == known.hash);
    REQUIRE(p.selectedState().request.sourceHashes == known.request.sourceHashes);
}
TEST(restoring_empty_state_clears_old_and_pending_ir_without_audio_allocation)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> ir(1, 64);
    ir.clear();
    ir.setSample(0, 0, 0.25f);
    auto file = tmp.wav("known.wav", ir);
    ch::PluginProcessor p(juce::File{}), blank(juce::File{});
    p.prepareToPlay(48000, 256);
    setParam(p, "mix", 100);
    p.loadIr(file);
    waitProcessor(p);
    juce::AudioBuffer<float> audio(2, 256);
    audio.clear();
    audio.setSample(0, 0, 1);
    juce::MidiBuffer midi;
    p.processBlock(audio, midi);
    test::near(audio.getSample(0, 0), 0.25f);
    p.setRouting({});
    juce::MemoryBlock empty;
    blank.getStateInformation(empty);
    p.setStateInformation(empty.getData(), int(empty.getSize()));
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        audio.clear();
        audio.setSample(0, 0, 1);
        {
            test::RealtimeGuard guard;
            p.processBlock(audio, midi);
        }
        REQUIRE(test::realtimeAllocations == 0);
        test::near(audio.getSample(0, 0), 1);
        REQUIRE(!p.reverb.loader().hasActive());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(!p.reverb.loader().status().ir);
}
