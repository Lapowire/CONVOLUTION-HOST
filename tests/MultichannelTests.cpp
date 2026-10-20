#include "RealtimeGuard.h"
#include "TestSupport.h"
#include "WavFixtures.h"
#include "plugin/PluginProcessor.h"
#include <chrono>
namespace
{
void wait(ch::PluginProcessor &p)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (p.reverb.loader().status().loading && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    REQUIRE(!p.reverb.loader().status().loading);
}
void layout(ch::PluginProcessor &p, juce::AudioChannelSet s)
{
    auto b = p.getBusesLayout();
    b.inputBuses.set(0, s);
    b.outputBuses.set(0, s);
    REQUIRE(p.setBusesLayout(b));
}
void param(ch::PluginProcessor &p, const char *id, float value)
{
    auto *v = p.parameters.getParameter(id);
    v->setValueNotifyingHost(v->convertTo0to1(value));
}
} // namespace
TEST(multichannel_layout_negotiation_rejects_asymmetry)
{
    ch::PluginProcessor p(juce::File{});
    for (auto s : {juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create7point1(),
                   juce::AudioChannelSet::create7point1point2(), juce::AudioChannelSet::create7point1point4(),
                   juce::AudioChannelSet::ambisonic(1), juce::AudioChannelSet::ambisonic(2),
                   juce::AudioChannelSet::ambisonic(3), juce::AudioChannelSet::discreteChannels(16)})
        layout(p, s);
    auto b = p.getBusesLayout();
    b.outputBuses.set(0, juce::AudioChannelSet::stereo());
    REQUIRE(!p.isBusesLayoutSupported(b));
    b.inputBuses.set(0, juce::AudioChannelSet::disabled());
    b.outputBuses.set(0, juce::AudioChannelSet::disabled());
    REQUIRE(!p.isBusesLayoutSupported(b));
    b.inputBuses.add(juce::AudioChannelSet::mono());
    REQUIRE(!p.isBusesLayoutSupported(b));
}
TEST(lfe_is_explicit_and_dry_remains_mix_dependent)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> ir(1, 1);
    ir.setSample(0, 0, 0.5f);
    auto file = tmp.wav("unit.wav", ir);
    for (bool enabled : {false, true})
        for (float mix : {0.f, 100.f})
        {
            ch::PluginProcessor p(juce::File{});
            layout(p, juce::AudioChannelSet::create5point1());
            p.setLfeWetEnabled(enabled);
            p.prepareToPlay(48000, 256);
            param(p, "mix", mix);
            p.loadIr(file);
            wait(p);
            REQUIRE(p.reverb.loader().status().error.isEmpty());
            auto bus = p.busLayout();
            REQUIRE(bus);
            int lfe = bus->lfe;
            juce::AudioBuffer<float> audio(6, 256);
            audio.clear();
            audio.setSample(lfe, 0, 1);
            juce::MidiBuffer midi;
            {
                test::RealtimeGuard guard;
                p.processBlock(audio, midi);
            }
            REQUIRE(test::realtimeAllocations == 0);
            for (int c = 0; c < 6; ++c)
                test::near(audio.getSample(c, 0), c == lfe ? (mix == 0 ? 1.f : enabled ? 0.5f : 0.f) : 0.f);
        }
}
TEST(ambix_preserves_acn_without_width_processing)
{
    test::TempDir tmp;
    for (int order : {1, 2, 3})
        for (float width : {0.f, 100.f, 200.f})
        {
            auto bus = juce::AudioChannelSet::ambisonic(order);
            int n = bus.size();
            juce::AudioBuffer<float> ir(n, 1);
            for (int c = 0; c < n; ++c)
                ir.setSample(c, 0, float(c + 1) / 32);
            auto file = tmp.wav("ambix.wav", ir);
            ch::PluginProcessor p(juce::File{});
            layout(p, bus);
            p.prepareToPlay(48000, 256);
            param(p, "mix", 100);
            param(p, "width", width);
            p.loadIr(file);
            wait(p);
            if (p.reverb.loader().status().error.isNotEmpty())
                std::cerr << "AmbiX " << order << ": " << p.reverb.loader().status().error << '\n';
            REQUIRE(p.reverb.loader().status().error.isEmpty());
            for (int input = 0; input < n; ++input)
            {
                p.reset();
                juce::AudioBuffer<float> audio(n, 256);
                audio.clear();
                audio.setSample(input, 0, 1);
                juce::MidiBuffer midi;
                {
                    test::RealtimeGuard guard;
                    p.processBlock(audio, midi);
                }
                REQUIRE(test::realtimeAllocations == 0);
                for (int c = 0; c < n; ++c)
                    test::near(audio.getSample(c, 0), c == input ? float(c + 1) / 32 : 0);
            }
        }
}
TEST(empty_explicit_matrix_and_layout_mismatch_survive_restore)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> ir(1, 1);
    ir.setSample(0, 0, 1);
    auto file = tmp.wav("unit.wav", ir);
    ch::PluginProcessor p(juce::File{});
    layout(p, juce::AudioChannelSet::discreteChannels(4));
    p.prepareToPlay(48000, 256);
    param(p, "mix", 100);
    p.loadIr(file, ch::RoutingModel(std::vector<ch::Route>{}));
    wait(p);
    REQUIRE(p.reverb.loader().status().error.isEmpty());
    juce::MemoryBlock state;
    p.getStateInformation(state);
    ch::PluginProcessor q(juce::File{});
    layout(q, juce::AudioChannelSet::discreteChannels(4));
    q.setStateInformation(state.getData(), int(state.getSize()));
    q.prepareToPlay(48000, 256);
    wait(q);
    juce::AudioBuffer<float> audio(4, 256);
    audio.clear();
    audio.setSample(0, 0, 1);
    juce::MidiBuffer midi;
    q.processBlock(audio, midi);
    test::near(audio.getMagnitude(0, 256), 0);
    REQUIRE(!q.selectedState().routing.automatic);
    ch::PluginProcessor wrong(juce::File{});
    layout(wrong, juce::AudioChannelSet::ambisonic(1));
    wrong.setStateInformation(state.getData(), int(state.getSize()));
    wrong.prepareToPlay(48000, 256);
    wait(wrong);
    REQUIRE(wrong.reverb.loader().status().error.isNotEmpty());
}
TEST(layout_change_supersedes_pending_ir_and_preserves_semantic_routing)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> ir(1, 64);
    ir.clear();
    ir.setSample(0, 0, 0.5f);
    auto file = tmp.wav("unit.wav", ir);
    ch::PluginProcessor p(juce::File{});
    p.prepareToPlay(48000, 256);
    p.loadIr(file);
    p.releaseResources();
    layout(p, juce::AudioChannelSet::ambisonic(3));
    p.prepareToPlay(96000, 64);
    wait(p);
    REQUIRE(p.reverb.loader().status().error.isEmpty());
    param(p, "mix", 100);
    juce::AudioBuffer<float> audio(16, 64);
    audio.clear();
    audio.setSample(15, 0, 1);
    juce::MidiBuffer midi;
    {
        test::RealtimeGuard guard;
        p.processBlock(audio, midi);
    }
    REQUIRE(test::realtimeAllocations == 0);
    REQUIRE(audio.getMagnitude(0, 0, 64) < 1e-5f);
    REQUIRE(audio.getMagnitude(15, 0, 64) > 0.1f);
}
