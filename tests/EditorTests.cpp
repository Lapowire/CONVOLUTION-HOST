#include "TestSupport.h"
#include "WavFixtures.h"
#include "plugin/PluginProcessor.h"
#include "ui/IrDisplay.h"
#include "ui/RoutingPanel.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
static juce::Component *findControl(juce::Component &parent, const juce::String &id)
{
    if (parent.getComponentID() == id)
        return &parent;
    for (auto *child : parent.getChildren())
        if (auto *found = findControl(*child, id))
            return found;
    return nullptr;
}
TEST(editor_size_controls_and_lifetime)
{
    test::TempDir tmp;
    ch::PluginProcessor processor(tmp.dir.getChildFile("prefs.json"));
    processor.prepareToPlay(48000, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    REQUIRE(editor->getWidth() == 1180);
    REQUIRE(editor->getHeight() == 760);
    auto *mix = dynamic_cast<juce::Slider *>(findControl(*editor, "mix"));
    REQUIRE(mix);
    mix->setValue(55, juce::sendNotificationSync);
    test::near(processor.parameters.getRawParameterValue("mix")->load(), 55);
    editor->setSize(960, 640);
    REQUIRE(mix->getBounds().getWidth() > 0);
    editor->setSize(1180, 760);
    juce::AudioBuffer<float> ir(1, 4800);
    for (int i = 0; i < 4800; ++i)
        ir.setSample(0, i, float(std::exp(-i / 700.) * std::sin(i * 0.3)));
    auto file = tmp.wav("Chamber.wav", ir);
    processor.loadIr(file);
    processor.library.addRoot(tmp.dir, true);
    editor.reset();
    editor.reset(processor.createEditor());
    for (int i = 0; i < 40 && processor.reverb.loader().status().loading; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    REQUIRE(processor.reverb.loader().status().ir);
    auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
    REQUIRE(snapshot.isValid());
    auto imageFile = juce::File::getCurrentWorkingDirectory().getChildFile("ui-preview.png");
    auto output = imageFile.createOutputStream();
    REQUIRE(output);
    REQUIRE(juce::PNGImageFormat{}.writeImageToStream(snapshot, *output));
    auto *bypass = dynamic_cast<juce::Button *>(findControl(*editor, "bypass"));
    REQUIRE(bypass);
    processor.library.scanAsync();
    editor.reset();
}
TEST(editor_all_pages_fit_at_minimum_size_and_disable_nyquist_bands)
{
    test::TempDir tmp;
    ch::PluginProcessor p(tmp.dir.getChildFile("prefs.json"));
    p.setRateAndBufferSizeDetails(8000, 256);
    p.prepareToPlay(8000, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    editor->setSize(960, 640);
    auto *tabs = dynamic_cast<juce::TabbedComponent *>(findControl(*editor, "tabs"));
    REQUIRE(tabs);
    tabs->setCurrentTabIndex(1);
    auto *trim = findControl(*editor, "trimStart");
    REQUIRE(trim);
    REQUIRE(trim->getWidth() > 0);
    REQUIRE(trim->getParentComponent()->getLocalBounds().contains(trim->getBounds()));
    tabs->setCurrentTabIndex(2);
    auto *routing = tabs->getTabContentComponent(2);
    REQUIRE(routing);
    for (auto *child : routing->getChildren())
        REQUIRE(routing->getLocalBounds().contains(child->getBounds()));
    tabs->setCurrentTabIndex(0);
    auto *high = findControl(*editor, "eq9");
    REQUIRE(high);
    REQUIRE(!high->isEnabled());
}
TEST(sparse_routing_apply_and_open_editor_state_changes_preserve_assignments)
{
    ch::PluginProcessor p(juce::File{});
    p.loadIr(juce::File("/tmp/routing.wav"), {{{1, 1, 3, 0.5f}}});
    ch::RoutingPanel panel(p);
    auto apply = [&]
    {
        for (auto *c : panel.getChildren())
            if (auto *b = dynamic_cast<juce::TextButton *>(c); b && b->getButtonText() == "Apply routing")
                b->onClick();
    };
    apply();
    auto state = p.selectedState();
    REQUIRE(state.routing.routes.size() == 1);
    REQUIRE(state.routing.routes[0].input == 1);
    REQUIRE(state.routing.routes[0].output == 1);
    REQUIRE(state.routing.routes[0].irChannel == 3);
    test::near(state.routing.routes[0].gain, 0.5f, 0.01f);
    p.setRouting({{{0, 1, 2, 0.75f}}});
    juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
    apply();
    state = p.selectedState();
    REQUIRE(state.routing.routes.size() == 1);
    REQUIRE(state.routing.routes[0].input == 0);
    REQUIRE(state.routing.routes[0].output == 1);
    REQUIRE(state.routing.routes[0].irChannel == 2);
}
TEST(waveform_selection_uses_original_source_time_after_trim)
{
    test::TempDir tmp;
    juce::AudioBuffer<float> source(1, 48000);
    source.clear();
    source.setSample(0, 24000, 1);
    ch::IrRequest request{tmp.wav("full.wav", source)};
    request.startMs = 400;
    request.endMs = 800;
    auto decoded = ch::IrDecoder{}.decode(request);
    REQUIRE(decoded);
    ch::IrDisplay display;
    display.setSize(600, 200);
    display.setIr(decoded.ir);
    display.setTrim(400, 800);
    float start = -1, end = -1;
    display.onSelection = [&](float a, float b)
    {
        start = a;
        end = b;
    };
    auto event = [&](float x)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {x, 100},
                                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
                                &display, &display, juce::Time::getCurrentTime(), {300, 100},
                                juce::Time::getCurrentTime(), 1, true);
    };
    display.mouseDown(event(300));
    display.mouseUp(event(440));
    test::near(start, 500, 0.01f);
    test::near(end, 750, 0.01f);
}
