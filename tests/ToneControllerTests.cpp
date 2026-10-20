#include "TestSupport.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ToneController.h"
TEST(tone_drag_is_one_undo_and_ab_copy_preserves_unrelated_parameters)
{
    ch::PluginProcessor p(juce::File{});auto& tone=p.toneController();
    tone.beginGesture();for(int i=0;i<100;++i)tone.setParameter("eq5",float(i)/20);tone.endGesture();
    test::near(p.parameters.getRawParameterValue("eq5")->load(),4.95f);
    REQUIRE(tone.canUndo());tone.undo();test::near(p.parameters.getRawParameterValue("eq5")->load(),0);REQUIRE(!tone.canUndo());
    tone.redo();test::near(p.parameters.getRawParameterValue("eq5")->load(),4.95f);
    tone.selectSlot(1);test::near(p.parameters.getRawParameterValue("eq5")->load(),0);
    tone.selectSlot(0);test::near(p.parameters.getRawParameterValue("eq5")->load(),4.95f);
    tone.copySlot(0,1);tone.selectSlot(1);test::near(p.parameters.getRawParameterValue("eq5")->load(),4.95f);
    test::near(p.parameters.getRawParameterValue("mix")->load(),25);
    juce::MemoryBlock bytes;p.getStateInformation(bytes);ch::PluginProcessor restored(juce::File{});restored.setStateInformation(bytes.getData(),int(bytes.getSize()));
    REQUIRE(restored.toneController().snapshot().active==1);
}
TEST(external_automation_during_gesture_is_not_overwritten_by_undo)
{
    ch::PluginProcessor p(juce::File{});auto& t=p.toneController();
    t.beginGesture();t.setParameter("eq3",6);t.setParameter("eq4",4);t.endGesture();
    auto* automated=p.parameters.getParameter("eq3");automated->setValueNotifyingHost(automated->convertTo0to1(9));
    t.undo();test::near(p.parameters.getRawParameterValue("eq3")->load(),9);test::near(p.parameters.getRawParameterValue("eq4")->load(),0);
    t.redo();test::near(p.parameters.getRawParameterValue("eq3")->load(),9);test::near(p.parameters.getRawParameterValue("eq4")->load(),4);
}
