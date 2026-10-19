#include "TestSupport.h"
#include "WavFixtures.h"
#include "plugin/PluginProcessor.h"
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
