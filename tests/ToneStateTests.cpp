#include "TestSupport.h"
#include "state/ToneState.h"
#include "state/StateStore.h"
TEST(tone_state_roundtrip_and_invalid_nodes_are_rejected)
{
    ch::ToneState tone;
    tone.eq.gainDb[3]=-5;tone.eq.bands[3].frequencyHz=777;tone.eq.bands[3].type=ch::EqType::highShelf;
    tone.damping.nodes[4].attenuationDbPerSecond=24;
    auto parsed=ch::ToneState::fromParameters(tone.parametersTree(),true);
    REQUIRE(parsed&&*parsed==tone);
    auto invalid=tone.parametersTree();
    for(auto child:invalid)if(child["id"].toString()=="damp1Freq")child.setProperty("value",20,nullptr);
    REQUIRE(!ch::ToneState::fromParameters(invalid,true));
    ch::SavedState s;s.parameters=tone.parametersTree();s.toneMemories.slots[0]=tone;
    ch::SavedState out;auto encoded=ch::StateStore::encode(s);
    REQUIRE(int(encoded["version"])==3);REQUIRE(ch::StateStore::decode(encoded,out));
    REQUIRE(out.toneMemories.slots[0]==tone);
}
