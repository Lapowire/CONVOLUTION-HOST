#include "TestSupport.h"
#include "plugin/PluginProcessor.h"
#include "plugin/ToneController.h"
TEST(tone_drag_is_one_undo_and_ab_copy_preserves_unrelated_parameters)
{
    ch::PluginProcessor p(juce::File{});
    auto &tone = p.toneController();
    tone.beginGesture();
    for (int i = 0; i < 100; ++i)
        tone.setParameter("eq5", float(i) / 20);
    tone.endGesture();
    test::near(p.parameters.getRawParameterValue("eq5")->load(), 4.95f);
    REQUIRE(tone.canUndo());
    tone.undo();
    test::near(p.parameters.getRawParameterValue("eq5")->load(), 0);
    REQUIRE(!tone.canUndo());
    tone.redo();
    test::near(p.parameters.getRawParameterValue("eq5")->load(), 4.95f);
    tone.selectSlot(1);
    test::near(p.parameters.getRawParameterValue("eq5")->load(), 0);
    tone.selectSlot(0);
    test::near(p.parameters.getRawParameterValue("eq5")->load(), 4.95f);
    tone.copySlot(0, 1);
    tone.selectSlot(1);
    test::near(p.parameters.getRawParameterValue("eq5")->load(), 4.95f);
    test::near(p.parameters.getRawParameterValue("mix")->load(), 25);
    juce::MemoryBlock bytes;
    p.getStateInformation(bytes);
    ch::PluginProcessor restored(juce::File{});
    restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
    REQUIRE(restored.toneController().snapshot().active == 1);
}
TEST(external_automation_during_gesture_is_not_overwritten_by_undo)
{
    ch::PluginProcessor p(juce::File{});
    auto &t = p.toneController();
    t.beginGesture();
    t.setParameter("eq3", 6);
    t.setParameter("eq4", 4);
    t.endGesture();
    auto *automated = p.parameters.getParameter("eq3");
    automated->setValueNotifyingHost(automated->convertTo0to1(9));
    t.undo();
    test::near(p.parameters.getRawParameterValue("eq3")->load(), 9);
    test::near(p.parameters.getRawParameterValue("eq4")->load(), 0);
    t.redo();
    test::near(p.parameters.getRawParameterValue("eq3")->load(), 9);
    test::near(p.parameters.getRawParameterValue("eq4")->load(), 4);
}
TEST(tone_host_gestures_are_balanced_and_unchanged_values_are_silent)
{
    struct Listener : juce::AudioProcessorParameter::Listener
    {
        int begins = 0, ends = 0, values = 0;
        void parameterValueChanged(int, float) override { ++values; }
        void parameterGestureChanged(int, bool start) override
        {
            if (start)
                ++begins;
            else
                ++ends;
        }
    } listener;
    ch::PluginProcessor p(juce::File{});
    auto *parameter = p.parameters.getParameter("eq5");
    parameter->addListener(&listener);
    auto &t = p.toneController();
    t.setParameter("eq5", 0);
    REQUIRE(listener.begins == 0);
    t.beginGesture();
    t.setParameter("eq5", 1);
    t.setParameter("eq5", 2);
    t.endGesture();
    REQUIRE(listener.begins == 1);
    REQUIRE(listener.ends == 1);
    REQUIRE(listener.values >= 2);
    parameter->removeListener(&listener);
}
TEST(panel_reset_does_not_write_concurrent_automation_in_other_panel)
{
    struct Listener : juce::AudioProcessorParameter::Listener
    {
        juce::RangedAudioParameter *other = nullptr;
        float target = 0;
        void parameterValueChanged(int, float) override
        {
            other->setValueNotifyingHost(other->convertTo0to1(target));
        }
        void parameterGestureChanged(int, bool) override {}
    } listener;
    ch::PluginProcessor p(juce::File{});
    auto &controller = p.toneController();
    controller.setParameter("eq5", 6);
    controller.setParameter("damp0Rate", 12);
    auto *eq = p.parameters.getParameter("eq5");
    auto *damping = p.parameters.getParameter("damp0Rate");
    listener.other = damping;
    listener.target = 25;
    eq->addListener(&listener);
    controller.resetEq();
    eq->removeListener(&listener);
    test::near(controller.current().damping.nodes[0].attenuationDbPerSecond, 25);
    listener.other = eq;
    listener.target = 9;
    damping->addListener(&listener);
    controller.resetDamping();
    damping->removeListener(&listener);
    test::near(controller.current().eq.gainDb[5], 9);
}
TEST(tone_history_has_a_bounded_128_gesture_capacity)
{
    ch::PluginProcessor p(juce::File{});
    auto &controller = p.toneController();
    for (int i = 0; i < 129; ++i)
        controller.setParameter("eq0", i % 2 ? -6 : 6);
    for (int i = 0; i < 128; ++i)
    {
        REQUIRE(controller.canUndo());
        controller.undo();
    }
    REQUIRE(!controller.canUndo());
    test::near(controller.current().eq.gainDb[0], 6);
}
