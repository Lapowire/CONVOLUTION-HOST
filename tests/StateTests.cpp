#include "TestSupport.h"
#include "WavFixtures.h"
#include "state/StateStore.h"
#include <juce_cryptography/juce_cryptography.h>
TEST(state_roundtrip_version_validation_and_mapping)
{
    ch::SavedState s;
    s.parameters = juce::ValueTree("PARAMETERS");
    s.parameters.setProperty("mix", 73, nullptr);
    s.request.file = juce::File("/tmp/ir.wav");
    s.request.startMs = 1;
    s.request.endMs = 8;
    s.request.fadeOutMs = 2;
    s.request.normalise = true;
    s.hash = "abc";
    s.relativePath = "Rooms/ir.wav";
    s.routing = {{{0, 0, 0, 1}, {0, 1, 1, 0.5f}}};
    auto tree = ch::StateStore::encode(s);
    ch::SavedState out;
    REQUIRE(ch::StateStore::decode(tree, out));
    REQUIRE(int(out.parameters["mix"]) == 73);
    REQUIRE(out.request.file == s.request.file);
    REQUIRE(out.request.normalise);
    REQUIRE(out.request.fadeOutMs == 2);
    REQUIRE(out.routing.routes.size() == 2);
    REQUIRE(out.hash == s.hash);
    tree.setProperty("version", 999, nullptr);
    REQUIRE(!ch::StateStore::decode(tree, out));
    REQUIRE(!ch::StateStore::decode({}, out));
}
TEST(state_relocation_checks_content_hash_not_basename)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> b(1, 1);
    b.setSample(0, 0, 0.25f);
    auto original = tmp.wav("ir.wav", b);
    ch::SavedState s;
    s.request.file = tmp.dir.getChildFile("lost/ir.wav");
    auto stream = original.createInputStream();
    s.hash = juce::SHA256(*stream).toHexString();
    s.relativePath = "ir.wav";
    REQUIRE(ch::StateStore::resolve(s, {tmp.dir.getFullPathName()}) == original);
    b.setSample(0, 0, 0.75f);
    tmp.wav("ir.wav", b);
    REQUIRE(!ch::StateStore::resolve(s, {tmp.dir.getFullPathName()}).existsAsFile());
}
TEST(state_keeps_four_mono_source_references)
{
    ch::SavedState s;
    s.parameters = juce::ValueTree("PARAMETERS");
    s.request.file = juce::File("/tmp/LL.wav");
    for (auto name : {"LR.wav", "RL.wav", "RR.wav"})
        s.request.additionalFiles.push_back(juce::File("/tmp").getChildFile(name));
    s.request.sourceHashes = {"ll", "lr", "rl", "rr"};
    s.request.additionalRelativePaths = {"LR.wav", "RL.wav", "RR.wav"};
    s.hash = "combined";
    ch::SavedState out;
    REQUIRE(ch::StateStore::decode(ch::StateStore::encode(s), out));
    REQUIRE(out.request.additionalFiles.size() == 3);
    REQUIRE(out.request.sourceHashes == s.request.sourceHashes);
}
TEST(state_v2_migrates_v1_and_keeps_empty_matrix_and_layout)
{
    ch::SavedState s;
    s.parameters = juce::ValueTree("PARAMETERS");
    s.routing = ch::RoutingModel(std::vector<ch::Route>{});
    s.routing.sourceLayout = "ambix-acn-sn3d-3";
    s.interpretation = ch::BusInterpretation::ambix;
    s.routing.lfeWetEnabled = true;
    auto tree = ch::StateStore::encode(s);
    REQUIRE(int(tree["version"]) == 2);
    ch::SavedState out;
    REQUIRE(ch::StateStore::decode(tree, out));
    REQUIRE(!out.routing.automatic);
    REQUIRE(out.routing.sourceLayout == s.routing.sourceLayout);
    REQUIRE(out.routing.lfeWetEnabled);
    REQUIRE(out.interpretation == s.interpretation);
    tree.setProperty("interpretation", 99, nullptr);
    REQUIRE(!ch::StateStore::decode(tree, out));
    tree.setProperty("version", 1, nullptr);
    tree.removeProperty("interpretation", nullptr);
    REQUIRE(ch::StateStore::decode(tree, out));
    REQUIRE(out.routing.automatic);
    juce::ValueTree route("ROUTE");
    route.setProperty("input", 0, nullptr);
    route.setProperty("output", 1, nullptr);
    route.setProperty("irChannel", 0, nullptr);
    route.setProperty("gain", -0.5, nullptr);
    tree.getChildWithName("IR").addChild(route, -1, nullptr);
    REQUIRE(ch::StateStore::decode(tree, out));
    REQUIRE(!out.routing.automatic);
    REQUIRE(out.routing.sourceLayout == "legacy-stereo");
    test::near(out.routing.routes[0].gain, -0.5f);
}
