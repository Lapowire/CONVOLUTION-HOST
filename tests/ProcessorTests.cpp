#include "TestSupport.h"
#include "WavFixtures.h"
#include "RealtimeGuard.h"
#include "plugin/PluginProcessor.h"
#include <chrono>
static void setParam(ch::PluginProcessor& p,const char* id,float value){auto* param=p.parameters.getParameter(id);REQUIRE(param);param->setValueNotifyingHost(param->convertTo0to1(value));}
TEST(plugin_parameters_layout_and_state) {
 test::TempDir tmp;ch::PluginProcessor p(tmp.dir.getChildFile("prefs.json"));
 REQUIRE(p.isBusesLayoutSupported(p.getBusesLayout()));auto bad=p.getBusesLayout();bad.inputBuses.set(0,juce::AudioChannelSet::create5point1());bad.outputBuses.set(0,juce::AudioChannelSet::create5point1());REQUIRE(!p.isBusesLayoutSupported(bad));
 REQUIRE(p.parameters.getParameter("mix"));setParam(p,"mix",73);setParam(p,"eq9",-6);setParam(p,"trimStart",2);
 juce::MemoryBlock state;p.getStateInformation(state);REQUIRE(state.getSize()>0);
 ch::PluginProcessor restored(tmp.dir.getChildFile("other.json"));restored.setStateInformation(state.getData(),int(state.getSize()));
 test::near(restored.parameters.getRawParameterValue("mix")->load(),73);test::near(restored.parameters.getRawParameterValue("eq9")->load(),-6);
 restored.setStateInformation("invalid",7);test::near(restored.parameters.getRawParameterValue("mix")->load(),73);
}
TEST(plugin_renders_ir_without_audio_allocation_and_restores_before_prepare) {
 test::TempDir tmp;juce::AudioBuffer<float> ir(1,128);ir.clear();ir.setSample(0,0,0.5f);ir.setSample(0,127,0.25f);auto file=tmp.wav("Room.wav",ir);
 ch::PluginProcessor p(tmp.dir.getChildFile("prefs.json"));setParam(p,"mix",100);p.loadIr(file);juce::MemoryBlock state;p.getStateInformation(state);
 ch::PluginProcessor restored(tmp.dir.getChildFile("other.json"));restored.setStateInformation(state.getData(),int(state.getSize()));restored.prepareToPlay(48000,256);
 auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(restored.reverb.loader().status().loading && std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(2));
 REQUIRE(restored.reverb.loader().status().error.isEmpty());juce::AudioBuffer<float> audio(2,256);audio.clear();audio.setSample(0,0,1);juce::MidiBuffer midi;
 {test::RealtimeGuard guard;restored.processBlock(audio,midi);}REQUIRE(test::realtimeAllocations==0);test::near(audio.getSample(0,0),0.5f);test::near(audio.getSample(0,127),0.25f);test::near(audio.getMagnitude(1,0,256),0);
 REQUIRE(restored.getLatencySamples()==0);REQUIRE(restored.getTailLengthSeconds()>=128./48000);
}
