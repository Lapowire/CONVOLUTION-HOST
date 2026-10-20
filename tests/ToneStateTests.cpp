#include "TestSupport.h"
#include "state/StateStore.h"
#include "state/ToneState.h"
TEST(tone_state_roundtrip_and_invalid_nodes_are_rejected)
{
    ch::ToneState tone;
    tone.eq.gainDb[3] = -5;
    tone.eq.bands[3].frequencyHz = 777;
    tone.eq.bands[3].type = ch::EqType::highShelf;
    tone.damping.nodes[4].attenuationDbPerSecond = 24;
    auto parsed = ch::ToneState::fromParameters(tone.parametersTree(), true);
    REQUIRE(parsed && *parsed == tone);
    auto invalid = tone.parametersTree();
    for (auto child : invalid)
        if (child["id"].toString() == "damp1Freq")
            child.setProperty("value", 20, nullptr);
    REQUIRE(!ch::ToneState::fromParameters(invalid, true));
    ch::SavedState s;
    s.parameters = tone.parametersTree();
    s.toneMemories.slots[0] = tone;
    ch::SavedState out;
    auto encoded = ch::StateStore::encode(s);
    REQUIRE(int(encoded["version"]) == 3);
    REQUIRE(ch::StateStore::decode(encoded, out));
    REQUIRE(out.toneMemories.slots[0] == tone);
}
TEST(tone_state_rejects_duplicate_parameters_enum_and_nan_without_mutation)
{
    auto original = ch::ToneState{}.parametersTree();
    for (auto id : {"peq0Type", "peq0Q", "dampProtect"})
    {
        auto bad = original.createCopy();
        for (auto child : bad)
            if (child["id"].toString() == id)
                child.setProperty(
                    "value", juce::String(id) == "peq0Type" ? 99. : std::numeric_limits<double>::quiet_NaN(),
                    nullptr);
        REQUIRE(!ch::ToneState::fromParameters(bad, true));
    }
    auto duplicate = original.createCopy();
    duplicate.addChild(duplicate.getChild(0).createCopy(), -1, nullptr);
    REQUIRE(!ch::ToneState::fromParameters(duplicate, true));
}
