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
#include "ui/MatrixEditor.h"
#include "ui/PluginEditor.h"
TEST(matrix_editor_all_channels_and_rows_are_reachable)
{
    auto layout =
        *ch::BusLayout::fromHost(juce::AudioChannelSet::ambisonic(3), ch::BusInterpretation::followHost);
    ch::MatrixEditor matrix;
    matrix.setSize(600, 100);
    ch::RoutingModel model(std::vector<ch::Route>{});
    for (int i = 0; i < 256; ++i)
        model.routes.push_back({i % 16, (i / 16) % 16, i % 16, i % 2 ? -0.5f : 0.5f});
    matrix.setModel(layout, model);
    auto result = matrix.model();
    REQUIRE(result.routes.size() == 256);
    test::near(result.routes.back().gain, -0.5f);
    auto *viewport = dynamic_cast<juce::Viewport *>(findControl(matrix, "matrixViewport"));
    REQUIRE(viewport);
    REQUIRE(viewport->getViewedComponent()->getHeight() > viewport->getHeight());
    viewport->setViewPosition(0, 100000);
    auto *last = dynamic_cast<juce::ComboBox *>(findControl(matrix, "route255.input"));
    REQUIRE(last);
    REQUIRE(last->getNumItems() == 16);
    REQUIRE(
        viewport->getViewedComponent()->getLocalBounds().contains(last->getParentComponent()->getBounds()));
    auto *remove = dynamic_cast<juce::Button *>(findControl(matrix, "matrixRemove"));
    REQUIRE(remove);
    remove->onClick();
    REQUIRE(matrix.model().routes.size() == 255);
    auto *add = dynamic_cast<juce::Button *>(findControl(matrix, "matrixAdd"));
    REQUIRE(add);
    add->onClick();
    REQUIRE(matrix.model().routes.size() == 256);
    add->onClick();
    REQUIRE(matrix.model().routes.size() == 256);
}
TEST(immersive_editor_controls_and_pending_preview_are_safe)
{
    test::TempDir tmp;
    ch::PluginProcessor p(juce::File{});
    auto buses = p.getBusesLayout();
    buses.inputBuses.set(0, juce::AudioChannelSet::create7point1point4());
    buses.outputBuses.set(0, juce::AudioChannelSet::create7point1point4());
    REQUIRE(p.setBusesLayout(buses));
    p.prepareToPlay(48000, 256);
    ch::PluginEditor editor(p);
    editor.setSize(960, 640);
    dynamic_cast<juce::TabbedComponent *>(findControl(editor, "tabs"))->setCurrentTabIndex(2);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    REQUIRE(!findControl(editor, "width")->isEnabled());
    auto *lfe = dynamic_cast<juce::ToggleButton *>(findControl(editor, "lfeWet"));
    REQUIRE(lfe);
    lfe->setToggleState(true, juce::sendNotificationSync);
    REQUIRE(p.selectedState().routing.lfeWetEnabled);
    auto *interpretation = dynamic_cast<juce::ComboBox *>(findControl(editor, "busInterpretation"));
    REQUIRE(interpretation);
    REQUIRE(!interpretation->isItemEnabled(3));
    juce::AudioBuffer<float> ir(1, 64);
    ir.clear();
    ir.setSample(0, 0, 1);
    p.loadIr(tmp.wav("first.wav", ir));
    for (int i = 0; i < 50 && p.reverb.loader().status().loading; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    p.loadIr(tmp.dir.getChildFile("missing.wav"));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);
    auto *display = findControl(editor, "irDisplay");
    REQUIRE(display);
    REQUIRE(!display->isEnabled());
    auto *name = dynamic_cast<juce::Label *>(findControl(editor, "irName"));
    REQUIRE(name);
    REQUIRE(name->getText().contains("missing"));
    auto dir = tmp.dir.getChildFile("unwritable.chpreset");
    REQUIRE(dir.createDirectory());
    REQUIRE(editor.savePresetTo(dir).failed());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    auto *status = dynamic_cast<juce::Label *>(findControl(editor, "status"));
    REQUIRE(status);
    REQUIRE(status->getText().containsIgnoreCase("save"));
    auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    auto output =
        juce::File::getCurrentWorkingDirectory().getChildFile("immersive-preview.png").createOutputStream();
    REQUIRE(output);
    REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *output));
}
TEST(matrix_draft_survives_timer_ticks_and_applies_explicitly)
{
    ch::PluginProcessor p(juce::File{});
    auto buses = p.getBusesLayout();
    buses.inputBuses.set(0, juce::AudioChannelSet::discreteChannels(16));
    buses.outputBuses.set(0, juce::AudioChannelSet::discreteChannels(16));
    REQUIRE(p.setBusesLayout(buses));
    p.setRouting({{{0, 1, 0, -0.5f}}});
    ch::RoutingPanel panel(p);
    panel.setSize(600, 160);
    auto *input = dynamic_cast<juce::ComboBox *>(findControl(panel, "route0.input"));
    REQUIRE(input);
    input->setSelectedId(16, juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
    REQUIRE(input->getSelectedId() == 16);
    REQUIRE(p.selectedState().routing.routes[0].input == 0);
    for (auto *c : panel.getChildren())
        if (auto *button = dynamic_cast<juce::TextButton *>(c);
            button && button->getButtonText() == "Apply routing")
            button->onClick();
    REQUIRE(p.selectedState().routing.routes[0].input == 15);
    test::near(p.selectedState().routing.routes[0].gain, -0.5f);
}
TEST(ambix_editor_snapshot_has_all_channel_labels)
{
    test::TempDir tmp;
    ch::PluginProcessor p(juce::File{});
    auto buses = p.getBusesLayout();
    buses.inputBuses.set(0, juce::AudioChannelSet::ambisonic(3));
    buses.outputBuses.set(0, juce::AudioChannelSet::ambisonic(3));
    REQUIRE(p.setBusesLayout(buses));
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> impulse(16, 4800);
    for (int c = 0; c < 16; ++c)
        for (int i = 0; i < 4800; ++i)
            impulse.setSample(c, i,
                              float(std::sin(i * (0.1 + c * 0.01)) * std::exp(-i / (400. + c * 40)) * .5));
    p.loadIr(tmp.wav("Ambisonic Chamber.wav", impulse));
    for (int i = 0; i < 100 && p.reverb.loader().status().loading; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    REQUIRE(p.reverb.loader().status().error.isEmpty());
    ch::PluginEditor editor(p);
    editor.setSize(1180, 900);
    dynamic_cast<juce::TabbedComponent *>(findControl(editor, "tabs"))->setCurrentTabIndex(2);
    auto *mode = dynamic_cast<juce::ComboBox *>(findControl(editor, "routingMode"));
    REQUIRE(mode);
    mode->setSelectedId(4, juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    auto *input = dynamic_cast<juce::ComboBox *>(findControl(editor, "route15.input"));
    REQUIRE(input);
    REQUIRE(input->getItemText(15) == "ACN 15");
    auto image = editor.createComponentSnapshot(editor.getLocalBounds());
    auto output =
        juce::File::getCurrentWorkingDirectory().getChildFile("ambix-preview.png").createOutputStream();
    REQUIRE(output);
    REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *output));
}

TEST(stereo_matrix_gain_is_preserved_when_opened_and_applied)
{
    ch::PluginProcessor p(juce::File{});
    p.setRouting({{{0, 0, 0, 16.f}}});
    ch::RoutingPanel panel(p);
    panel.setSize(600, 180);
    auto *mode = dynamic_cast<juce::ComboBox *>(findControl(panel, "routingMode"));
    REQUIRE(mode && mode->getSelectedId() == 4);
    for (auto *c : panel.getChildren())
        if (auto *button = dynamic_cast<juce::TextButton *>(c);
            button && button->getButtonText() == "Apply routing")
            button->onClick();
    test::near(p.selectedState().routing.routes[0].gain, 16.f);
}
TEST(routing_panel_tracks_host_layout_changes_without_selection_edits)
{
    ch::PluginProcessor p(juce::File{});
    auto setLayout = [&](juce::AudioChannelSet channels)
    {
        auto buses = p.getBusesLayout();
        buses.inputBuses.set(0, channels);
        buses.outputBuses.set(0, channels);
        REQUIRE(p.setBusesLayout(buses));
    };
    setLayout(juce::AudioChannelSet::discreteChannels(4));
    p.setRouting({{{0, 0, 0, 1}}});
    ch::RoutingPanel panel(p);
    panel.setSize(600, 180);
    setLayout(juce::AudioChannelSet::create5point1());
    juce::MessageManager::getInstance()->runDispatchLoopUntil(250);
    auto *input = dynamic_cast<juce::ComboBox *>(findControl(panel, "route0.input"));
    REQUIRE(input && input->getNumItems() == 6);
    REQUIRE(input->getItemText(0) == "L");
    REQUIRE(dynamic_cast<juce::ToggleButton *>(findControl(panel, "lfeWet"))->isEnabled());
}
