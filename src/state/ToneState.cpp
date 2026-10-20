#include "ToneState.h"
#include <cmath>
#include <map>
namespace ch
{
std::vector<ToneParameter> toneParameterDefinitions(bool appendedOnly)
{
    std::vector<ToneParameter> result;
    auto add = [&](juce::String id, juce::String name, float lo, float hi, float def, float step = 0.01f,
                   bool log = false) { result.push_back({id, name, lo, hi, def, step, log}); };
    if (!appendedOnly)
        for (int i = 0; i < 10; ++i)
            add("eq" + juce::String(i), "EQ gain " + juce::String(i + 1), -12, 12, 0);
    for (int i = 0; i < 10; ++i)
    {
        auto id = "peq" + juce::String(i);
        auto label = "EQ " + juce::String(i + 1);
        add(id + "Freq", label + " frequency", 20, 20000, eqFrequencies[std::size_t(i)], 0.1f, true);
        add(id + "Q", label + " Q", 0.1f, 18, 1.4f, 0.01f, true);
        add(id + "Type", label + " type", 0, 3, 0, 1);
        add(id + "Enabled", label + " enabled", 0, 1, 1, 1);
    }
    add("eqBypass", "Post EQ bypass", 0, 1, 0, 1);
    DampingSettings defaults;
    for (int i = 0; i < 6; ++i)
    {
        auto id = "damp" + juce::String(i);
        auto label = "Damping " + juce::String(i + 1);
        add(id + "Freq", label + " frequency", 20, 20000, defaults.nodes[std::size_t(i)].frequencyHz, 0.1f,
            true);
        add(id + "Rate", label + " dB per second", 0, 60, 0);
    }
    add("dampProtect", "Early protection ms", 0, 200, 20, 0.1f);
    add("dampBypass", "Damping bypass", 0, 1, 0, 1);
    return result;
}
juce::ValueTree ToneState::parametersTree() const
{
    juce::ValueTree tree("PARAMETERS");
    auto add = [&](juce::String id, float value)
    {
        juce::ValueTree p("PARAM");
        p.setProperty("id", id, nullptr);
        p.setProperty("value", value, nullptr);
        tree.addChild(p, -1, nullptr);
    };
    for (int i = 0; i < 10; ++i)
    {
        auto n = std::size_t(i);
        auto prefix = "peq" + juce::String(i);
        add("eq" + juce::String(i), eq.gainDb[n]);
        add(prefix + "Freq", eq.bands[n].frequencyHz);
        add(prefix + "Q", eq.bands[n].q);
        add(prefix + "Type", float(eq.bands[n].type));
        add(prefix + "Enabled", eq.bands[n].enabled ? 1 : 0);
    }
    add("eqBypass", eq.bypass ? 1 : 0);
    for (int i = 0; i < 6; ++i)
    {
        auto n = std::size_t(i);
        auto prefix = "damp" + juce::String(i);
        add(prefix + "Freq", damping.nodes[n].frequencyHz);
        add(prefix + "Rate", damping.nodes[n].attenuationDbPerSecond);
    }
    add("dampProtect", damping.protectMs);
    add("dampBypass", damping.bypass ? 1 : 0);
    return tree;
}
std::optional<ToneState> ToneState::fromParameters(const juce::ValueTree &tree, bool requireAll)
{
    if (!tree.hasType("PARAMETERS"))
        return {};
    std::map<juce::String, float> values;
    for (const auto &def : toneParameterDefinitions())
    {
        bool found = false;
        float value = def.defaultValue;
        for (auto child : tree)
            if (child["id"].toString() == def.id)
            {
                if (found || !child.hasType("PARAM") || !child.hasProperty("value"))
                    return {};
                auto v = child["value"];
                if (!v.isInt() && !v.isInt64() && !v.isDouble() && !v.isBool())
                    return {};
                value = float(v);
                found = true;
                if (!std::isfinite(value) || value < def.minimum || value > def.maximum ||
                    (def.interval == 1 && std::floor(value) != value))
                    return {};
            }
        if (requireAll && !found)
            return {};
        values[def.id] = value;
    }
    ToneState state;
    for (int i = 0; i < 10; ++i)
    {
        auto n = std::size_t(i);
        auto prefix = "peq" + juce::String(i);
        state.eq.gainDb[n] = values["eq" + juce::String(i)];
        state.eq.bands[n] = {values[prefix + "Freq"], values[prefix + "Q"],
                             EqType(int(values[prefix + "Type"])), values[prefix + "Enabled"] != 0};
    }
    state.eq.bypass = values["eqBypass"] != 0;
    for (int i = 0; i < 6; ++i)
    {
        auto n = std::size_t(i);
        auto prefix = "damp" + juce::String(i);
        state.damping.nodes[n] = {values[prefix + "Freq"], values[prefix + "Rate"]};
    }
    state.damping.protectMs = values["dampProtect"];
    state.damping.bypass = values["dampBypass"] != 0;
    if (!state.damping.isValid())
        return {};
    return state;
}
juce::ValueTree ToneMemories::encode() const
{
    juce::ValueTree tree("TONE_MEMORIES");
    tree.setProperty("active", active, nullptr);
    for (auto &slot : slots)
        tree.addChild(slot.parametersTree(), -1, nullptr);
    return tree;
}
std::optional<ToneMemories> ToneMemories::decode(const juce::ValueTree &tree)
{
    if (!tree.hasType("TONE_MEMORIES") || tree.getNumChildren() != 2 || !tree.hasProperty("active"))
        return {};
    double active = double(tree["active"]);
    if (!std::isfinite(active) || active != std::floor(active) || active < 0 || active > 1)
        return {};
    ToneMemories m;
    m.active = int(active);
    for (int i = 0; i < 2; ++i)
    {
        auto slot = ToneState::fromParameters(tree.getChild(i), true);
        if (!slot)
            return {};
        m.slots[std::size_t(i)] = *slot;
    }
    return m;
}
} // namespace ch
