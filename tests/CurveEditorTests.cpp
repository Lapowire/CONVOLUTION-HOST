#include "TestSupport.h"
#include "plugin/PluginProcessor.h"
#include "ui/CurveEditor.h"
TEST(curve_keyboard_edits_tone_and_respects_nyquist)
{
    ch::PluginProcessor p(juce::File{});
    ch::CurveEditor curve(p.toneController(), ch::CurveMode::postEq);
    curve.setSize(600, 180);
    auto tone = p.toneController().current();
    curve.setDisplayState(tone, tone, 48000, false);
    REQUIRE(curve.selectedNode() == 0);
    REQUIRE(curve.keyPressed(juce::KeyPress(juce::KeyPress::upKey)));
    test::near(p.parameters.getRawParameterValue("eq0")->load(), 0.5f);
    p.toneController().undo();
    test::near(p.parameters.getRawParameterValue("eq0")->load(), 0);
    curve.setDisplayState(tone, tone, 8000, false);
    auto image = curve.createComponentSnapshot(curve.getLocalBounds());
    REQUIRE(image.isValid());
}
#include "ui/TonePanel.h"
static juce::Component *toneControl(juce::Component &parent, const juce::String &id)
{
    if (parent.getComponentID() == id)
        return &parent;
    for (auto *child : parent.getChildren())
        if (auto *found = toneControl(*child, id))
            return found;
    return nullptr;
}
TEST(tone_numeric_frequency_is_limited_to_nyquist_and_panels_render)
{
    ch::PluginProcessor p(juce::File{});
    p.setRateAndBufferSizeDetails(8000, 256);
    ch::TonePanel panel(p, ch::CurveMode::postEq);
    panel.setSize(638, 166);
    auto *frequency = dynamic_cast<juce::Slider *>(toneControl(panel, "eqFrequency"));
    REQUIRE(frequency);
    REQUIRE(frequency->getMaximum() < 4000);
    p.setRateAndBufferSizeDetails(48000, 256);
    auto &t = p.toneController();
    t.setParameter("eq5", 6);
    t.setParameter("peq5Freq", 1400);
    t.setParameter("peq5Q", 0.8f);
    t.setParameter("peq8Type", 2);
    t.setParameter("eq8", -4);
    for (int i = 0; i < 6; ++i)
        t.setParameter("damp" + juce::String(i) + "Rate", float(i * 6));
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto *tabs = dynamic_cast<juce::TabbedComponent *>(toneControl(*editor, "tabs"));
    REQUIRE(tabs);
    for (int size : {960, 1180})
    {
        editor->setSize(size, size == 960 ? 640 : 760);
        for (int tab : {0, 3})
        {
            tabs->setCurrentTabIndex(tab);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
            auto image = editor->createComponentSnapshot(editor->getLocalBounds());
            REQUIRE(image.isValid());
            auto file = juce::File::getCurrentWorkingDirectory().getChildFile(
                "tone-" + juce::String(tab) + "-" + juce::String(size) + ".png");
            auto out = file.createOutputStream();
            REQUIRE(out);
            REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *out));
        }
    }
}
TEST(curve_drag_is_one_gesture_and_damping_nodes_do_not_cross)
{
    ch::PluginProcessor p(juce::File{});auto& tone=p.toneController();
    ch::CurveEditor eq(tone,ch::CurveMode::postEq);eq.setSize(600,180);eq.selectNode(5);auto state=tone.current();eq.setDisplayState(state,state,48000,false);
    auto event=[&](juce::Component& c,juce::Point<float> point,bool dragged){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,dragged);};
    float x=42+float(std::log(1000./20)/std::log(1000.))*516;
    eq.mouseDown(event(eq,{x,81.5f},false));
    for(int i=0;i<50;++i)eq.mouseDrag(event(eq,{x+20+i*0.1f,60},true));
    eq.mouseUp(event(eq,{x+25,60},true));
    REQUIRE(tone.current().eq.gainDb[5]>4);REQUIRE(tone.current().eq.bands[5].frequencyHz>1000);
    tone.undo();test::near(tone.current().eq.gainDb[5],0);test::near(tone.current().eq.bands[5].frequencyHz,1000);REQUIRE(!tone.canUndo());
    ch::CurveEditor damping(tone,ch::CurveMode::damping);damping.setSize(600,180);damping.selectNode(2);state=tone.current();damping.setDisplayState(state,state,48000,false);
    damping.mouseDown(event(damping,{x,153},false));damping.mouseDrag(event(damping,{550,80},true));damping.mouseUp(event(damping,{550,80},true));
    REQUIRE(tone.current().damping.isValid());REQUIRE(tone.current().damping.nodes[2].frequencyHz<4000);REQUIRE(tone.current().damping.nodes[2].attenuationDbPerSecond>0);
}
