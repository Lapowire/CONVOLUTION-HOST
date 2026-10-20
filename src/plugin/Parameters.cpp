#include "Parameters.h"
namespace ch
{
namespace
{
constexpr std::array<const char *, 19> ids{
    "inputGain", "outputGain", "wetGain",        "mix",    "predelay",  "width",
    "highpass",  "lowpass",    "filtersEnabled", "bypass", "trimStart", "trimEnd",
    "fadeIn",    "fadeOut",    "normalise",      "eq0",    "eq1",       "eq2",
    "eq3"};
}
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&](const char *id, const char *label, float lo, float hi, float def, float step = 0.01f)
    {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, label, juce::NormalisableRange<float>{lo, hi, step}, def));
    };
    add("inputGain", "Input gain", -24, 24, 0);
    add("outputGain", "Output gain", -24, 24, 0);
    add("wetGain", "Wet gain", -60, 12, 0);
    add("mix", "Dry / wet", 0, 100, 25);
    add("predelay", "Predelay", 0, 500, 0);
    add("width", "Stereo width", 0, 200, 100);
    add("highpass", "Wet high-pass", 20, 2000, 20, 1);
    add("lowpass", "Wet low-pass", 1000, 20000, 20000, 1);
    for (int i = 0; i < 10; ++i)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"eq" + juce::String(i), 1},
            "EQ " + juce::String(eqFrequencies[std::size_t(i)]) + " Hz",
            juce::NormalisableRange<float>{-12, 12, 0.01f}, 0.f));
    add("trimStart", "IR start", 0, 30000, 0);
    add("trimEnd", "IR end (0 = full)", 0, 30000, 0);
    add("fadeIn", "IR fade-in", 0, 500, 0);
    add("fadeOut", "IR fade-out", 0, 5000, 0);
    for (const auto &item : std::array<std::pair<const char *, const char *>, 3>{
             {{"filtersEnabled", "Wet filters"}, {"bypass", "Bypass"}, {"normalise", "Normalise IR"}}})
        layout.add(
            std::make_unique<juce::AudioParameterBool>(juce::ParameterID{item.first, 1}, item.second, false));
    for (const auto &d : toneParameterDefinitions(true))
    {
        juce::NormalisableRange<float> range{d.minimum, d.maximum, d.interval};
        if (d.logarithmic)
            range.setSkewForCentre(std::sqrt(d.minimum * d.maximum));
        if (d.id.endsWith("Type"))
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID{d.id, 1}, d.name,
                juce::StringArray{"Bell", "Low shelf", "High shelf", "Notch"}, int(d.defaultValue)));
        else if (d.maximum == 1 && d.minimum == 0)
            layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{d.id, 1}, d.name,
                                                                  d.defaultValue != 0));
        else
            layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{d.id, 1}, d.name, range,
                                                                   d.defaultValue));
    }
    return layout;
}
ParameterRefs::ParameterRefs(juce::AudioProcessorValueTreeState &state)
{
    for (std::size_t i = 0; i < ids.size(); ++i)
        values[i] = state.getRawParameterValue(ids[i]);
    for (int i = 0; i < 10; ++i)
        bands[std::size_t(i)] = state.getRawParameterValue("eq" + juce::String(i));
    for (int i = 0; i < 10; ++i)
    {
        auto prefix = "peq" + juce::String(i);
        int j = 0;
        for (auto suffix : {"Freq", "Q", "Type", "Enabled"})
            shapes[std::size_t(i)][std::size_t(j++)] = state.getRawParameterValue(prefix + suffix);
    }
    for (int i = 0; i < 6; ++i)
    {
        auto prefix = "damp" + juce::String(i);
        damping[std::size_t(i)] = {state.getRawParameterValue(prefix + "Freq"),
                                   state.getRawParameterValue(prefix + "Rate")};
    }
    eqBypass = state.getRawParameterValue("eqBypass");
    dampProtect = state.getRawParameterValue("dampProtect");
    dampBypass = state.getRawParameterValue("dampBypass");
}
ParameterSnapshot ParameterRefs::read() const noexcept
{
    ParameterSnapshot p;
    auto v = [this](std::size_t i) { return values[i]->load(std::memory_order_relaxed); };
    p.inputGain = v(0);
    p.outputGain = v(1);
    p.wetGain = v(2);
    p.mix = v(3);
    p.predelay = v(4);
    p.width = v(5);
    p.highpass = v(6);
    p.lowpass = v(7);
    p.filtersEnabled = v(8) >= 0.5f;
    p.bypass = v(9) >= 0.5f;
    for (std::size_t i = 0; i < 10; ++i)
        p.eq[i] = bands[i]->load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < 10; ++i)
        p.eqShape[i] = {shapes[i][0]->load(), shapes[i][1]->load(), EqType(int(shapes[i][2]->load())),
                        shapes[i][3]->load() >= 0.5f};
    p.eqBypass = eqBypass->load() >= 0.5f;
    return p;
}
DampingSettings ParameterRefs::readDamping() const noexcept
{
    DampingSettings s;
    for (std::size_t i = 0; i < 6; ++i)
        s.nodes[i] = {damping[i][0]->load(), damping[i][1]->load()};
    s.protectMs = dampProtect->load();
    s.bypass = dampBypass->load() >= 0.5f;
    return s;
}
} // namespace ch
