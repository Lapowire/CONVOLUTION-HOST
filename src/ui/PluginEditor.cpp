#include "PluginEditor.h"
namespace ch
{
namespace
{
constexpr std::array<const char *, 6> mainIds{"predelay",  "mix",     "width",
                                              "inputGain", "wetGain", "outputGain"};
constexpr std::array<const char *, 6> mainLabels{"PREDELAY", "DRY / WET", "STEREO WIDTH",
                                                 "INPUT",    "WET LEVEL", "OUTPUT"};
constexpr std::array<const char *, 4> shapeIds{"trimStart", "trimEnd", "fadeIn", "fadeOut"};
constexpr std::array<const char *, 4> shapeNames{"START / ms", "END / ms", "FADE IN / ms", "FADE OUT / ms"};
void rotary(juce::Slider &s, const char *id, const char *suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 20);
    s.setComponentID(id);
    s.setTitle(id);
    s.setTextValueSuffix(suffix);
    s.setNumDecimalPlacesToDisplay(1);
    s.setWantsKeyboardFocus(true);
}
} // namespace
PluginEditor::PluginEditor(PluginProcessor &p)
    : AudioProcessorEditor(p), processor(p), library(p.library), routing(p)
{
    setLookAndFeel(&theme);
    setResizable(true, true);
    setResizeLimits(960, 640, 1800, 1200);
    for (auto *c :
         std::initializer_list<juce::Component *>{&library, &display, &open, &loadPreset, &savePreset,
                                                  &bypass, &decay, &irName, &status, &meter, &tabs})
        addAndMakeVisible(c);
    irName.setFont(ui::font(18, true));
    status.setFont(ui::font(11));
    status.setColour(juce::Label::textColourId, ui::muted);
    meter.setFont(ui::font(11));
    meter.setJustificationType(juce::Justification::centredRight);
    open.onClick = [this] { openIr(); };
    savePreset.onClick = [this] { preset(true); };
    loadPreset.onClick = [this] { preset(false); };
    bypass.setClickingTogglesState(true);
    bypass.setComponentID("bypass");
    bypass.setTitle("Bypass");
    buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.parameters, "bypass", bypass));
    decay.setClickingTogglesState(true);
    decay.onClick = [this] { display.setDecay(decay.getToggleState()); };
    library.onSelect = [this](const juce::File &f) { processor.loadIr(f); };
    display.onSelection = [this](float a, float b)
    {
        changeParameter("trimStart", a);
        changeParameter("trimEnd", b);
    };
    for (std::size_t i = 0; i < mainControls.size(); ++i)
    {
        auto &s = mainControls[i];
        rotary(s, mainIds[i], i == 0 ? " ms" : i < 3 ? " %" : " dB");
        s.setTooltip(mainLabels[i]);
        addAndMakeVisible(s);
        sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, mainIds[i], s));
        s.setDoubleClickReturnValue(true, i == 1 ? 25 : i == 2 ? 100 : 0);
    }
    tabs.setComponentID("tabs");
    tabs.setTabBarDepth(30);
    tabs.setOutline(0);
    tabs.addTab("GRAPHIC EQ", ui::panel, &eqPage, false);
    tabs.addTab("IR SHAPE", ui::panel, &shapePage, false);
    tabs.addTab("ROUTING", ui::panel, &routing, false);
    for (std::size_t i = 0; i < eqControls.size(); ++i)
    {
        auto &s = eqControls[i];
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 20);
        s.setComponentID("eq" + juce::String(int(i)));
        s.setTitle("EQ " + juce::String(eqFrequencies[i]) + " Hz");
        s.setTextValueSuffix(" dB");
        s.setNumDecimalPlacesToDisplay(1);
        s.setDoubleClickReturnValue(true, 0);
        eqPage.addAndMakeVisible(s);
        sliderAttachments.push_back(
            std::make_unique<SliderAttachment>(processor.parameters, "eq" + juce::String(int(i)), s));
        auto &label = eqLabels[i];
        label.setText(eqFrequencies[i] >= 1000 ? juce::String(eqFrequencies[i] / 1000, 0) + "k"
                                               : juce::String(eqFrequencies[i], i == 0 ? 1 : 0),
                      juce::dontSendNotification);
        label.setFont(ui::font(11));
        label.setJustificationType(juce::Justification::centred);
        eqPage.addAndMakeVisible(label);
    }
    for (std::size_t i = 0; i < shapeControls.size(); ++i)
    {
        rotary(shapeControls[i], shapeIds[i], " ms");
        shapePage.addAndMakeVisible(shapeControls[i]);
        sliderAttachments.push_back(
            std::make_unique<SliderAttachment>(processor.parameters, shapeIds[i], shapeControls[i]));
        shapeLabels[i].setText(shapeNames[i], juce::dontSendNotification);
        shapeLabels[i].setFont(ui::font(11));
        shapeLabels[i].setJustificationType(juce::Justification::centred);
        shapePage.addAndMakeVisible(shapeLabels[i]);
    }
    shapePage.addAndMakeVisible(normalise);
    shapePage.addAndMakeVisible(shapeHint);
    shapeHint.setFont(ui::font(11));
    shapeHint.setColour(juce::Label::textColourId, ui::muted);
    shapeHint.setText("End 0 = full IR. Drag across the waveform to trim.", juce::dontSendNotification);
    buttonAttachments.push_back(
        std::make_unique<ButtonAttachment>(processor.parameters, "normalise", normalise));
    for (auto *s : {&highpass, &lowpass})
    {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 78, 20);
        s->setTextValueSuffix(" Hz");
        s->setNumDecimalPlacesToDisplay(0);
        eqPage.addAndMakeVisible(s);
    }
    highpass.setTitle("Wet high-pass");
    highpass.setTooltip("High-pass filter");
    lowpass.setTitle("Wet low-pass");
    lowpass.setTooltip("Low-pass filter");
    eqPage.addAndMakeVisible(filtersEnabled);
    sliderAttachments.push_back(
        std::make_unique<SliderAttachment>(processor.parameters, "highpass", highpass));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, "lowpass", lowpass));
    buttonAttachments.push_back(
        std::make_unique<ButtonAttachment>(processor.parameters, "filtersEnabled", filtersEnabled));
    setSize(1180, 760);
    timerCallback();
    startTimerHz(20);
}
PluginEditor::~PluginEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}
void PluginEditor::changeParameter(const char *id, float v)
{
    if (auto *p = processor.parameters.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(v));
        p->endChangeGesture();
    }
}
void PluginEditor::paint(juce::Graphics &g)
{
    g.fillAll(ui::background);
    g.setColour(ui::accent);
    g.fillRoundedRectangle(24, 17, 32, 3, 1);
    g.setFont(ui::font(22, true));
    g.setColour(ui::text);
    g.drawText("CONVOLUTION", 24, 24, 235, 28, juce::Justification::left);
    g.setFont(ui::font(10, true));
    g.setColour(ui::muted);
    g.drawText("HOST  /  L A P O W I R E", 24, 53, 230, 18, juce::Justification::left);
    ui::card(g, mainArea.toFloat());
    g.setFont(ui::font(10, true));
    g.setColour(ui::muted);
    for (int i = 0; i < 6; ++i)
    {
        int width = mainArea.getWidth() / 6;
        g.drawText(mainLabels[std::size_t(i)], mainArea.getX() + i * width, mainArea.getY() + 10, width, 18,
                   juce::Justification::centred);
    }
}
void PluginEditor::resized()
{
    const int x = 278, w = getWidth() - x - 24;
    library.setBounds(24, 90, 230, getHeight() - 114);
    const int buttonY = 24;
    open.setBounds(getWidth() - 386, buttonY, 84, 30);
    loadPreset.setBounds(getWidth() - 294, buttonY, 64, 30);
    savePreset.setBounds(getWidth() - 222, buttonY, 64, 30);
    bypass.setBounds(getWidth() - 150, buttonY, 82, 30);
    irName.setBounds(x, 17, std::max(120, w - 410), 30);
    meter.setBounds(x, 51, w, 20);
    int waveHeight = int(getHeight() * 0.27);
    display.setBounds(x, 90, w, waveHeight);
    decay.setBounds(x + w - 88, 100, 70, 24);
    mainArea = {x, 90 + waveHeight + 16, w, 130};
    int cell = w / 6;
    for (int i = 0; i < 6; ++i)
        mainControls[std::size_t(i)].setBounds(x + i * cell + 6, mainArea.getY() + 28, cell - 12, 92);
    const int tabY = mainArea.getBottom() + 16;
    tabs.setBounds(x, tabY, w, getHeight() - tabY - 43);
    status.setBounds(x, getHeight() - 34, w, 22);
    const int ew = tabs.getWidth(), eh = tabs.getHeight() - 30;
    for (int i = 0; i < 10; ++i)
    {
        int cw = (ew - 24) / 10;
        eqLabels[std::size_t(i)].setBounds(12 + i * cw, 6, cw, 18);
        eqControls[std::size_t(i)].setBounds(12 + i * cw, 28, cw, std::max(50, eh - 68));
    }
    filtersEnabled.setBounds(16, eh - 32, 130, 26);
    highpass.setBounds(148, eh - 32, std::max(100, (ew - 170) / 2), 26);
    lowpass.setBounds(148 + (ew - 170) / 2, eh - 32, std::max(100, (ew - 170) / 2), 26);
    const int sw = ew, sh = eh;
    for (int i = 0; i < 4; ++i)
    {
        int cw = sw / 4;
        shapeLabels[std::size_t(i)].setBounds(i * cw, 8, cw, 18);
        shapeControls[std::size_t(i)].setBounds(i * cw + 14, 30, cw - 28,
                                                std::min(110, std::max(65, sh - 72)));
    }
    normalise.setBounds(16, sh - 34, 250, 26);
    shapeHint.setBounds(272, sh - 34, std::max(100, sw - 284), 26);
}
void PluginEditor::timerCallback()
{
    auto s = processor.reverb.loader().status();
    auto chosen = processor.selectedState();
    display.setIr(s.ir);
    irName.setText(s.ir                                  ? s.ir->file.getFileNameWithoutExtension()
                   : chosen.request.file != juce::File{} ? chosen.request.file.getFileNameWithoutExtension()
                                                         : "Choose your space",
                   juce::dontSendNotification);
    auto message = s.loading              ? juce::String("Preparing impulse response...")
                   : s.error.isNotEmpty() ? s.error
                   : s.ir                 ? juce::String(s.ir->originalSampleRate / 1000, 1) + " kHz / " +
                                juce::String(s.ir->samples.getNumChannels()) + " IR channels / " +
                                juce::String(s.ir->durationSeconds(), 3) + " s / zero-latency engine"
                          : "No IR loaded. Audio passes through unchanged.";
    status.setColour(juce::Label::textColourId, s.error.isEmpty() ? ui::muted : ui::amber);
    status.setText(message, juce::dontSendNotification);
    display.setTrim(float(chosen.request.startMs), float(chosen.request.endMs));
    auto in = processor.reverb.inputPeak.load(), out = processor.reverb.outputPeak.load();
    meter.setColour(juce::Label::textColourId, out > 1 ? ui::amber : ui::muted);
    meter.setText("IN " + juce::String(juce::Decibels::gainToDecibels(in, -100.f), 1) + " dB   /   OUT " +
                      juce::String(juce::Decibels::gainToDecibels(out, -100.f), 1) + " dB   /   " +
                      juce::String(processor.getTotalNumInputChannels()) + " CH",
                  juce::dontSendNotification);
    const double sr = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000;
    for (std::size_t i = 0; i < 10; ++i)
        eqControls[i].setEnabled(eqFrequencies[i] < sr * 0.5);
}
void PluginEditor::openIr()
{
    if (choosing)
        return;
    choosing = true;
    chooser = std::make_unique<juce::FileChooser>("Open WAV impulse response", juce::File{}, "*.wav", true);
    auto safe = juce::Component::SafePointer<PluginEditor>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser &fc)
                         {
                             if (!safe)
                                 return;
                             safe->choosing = false;
                             if (fc.getResult().existsAsFile())
                                 safe->processor.loadIr(fc.getResult());
                         });
}
void PluginEditor::preset(bool save)
{
    if (choosing)
        return;
    choosing = true;
    chooser = std::make_unique<juce::FileChooser>(save ? "Save preset" : "Load preset", juce::File{},
                                                  "*.chpreset", true);
    auto safe = juce::Component::SafePointer<PluginEditor>(this);
    chooser->launchAsync(
        (save ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting
              : juce::FileBrowserComponent::openMode) |
            juce::FileBrowserComponent::canSelectFiles,
        [safe, save](const juce::FileChooser &fc)
        {
            if (!safe)
                return;
            safe->choosing = false;
            auto file = fc.getResult();
            if (file == juce::File{})
                return;
            if (save)
            {
                juce::MemoryBlock state;
                safe->processor.getStateInformation(state);
                if (!file.withFileExtension("chpreset").replaceWithData(state.getData(), state.getSize()))
                    safe->status.setText("Could not save preset", juce::dontSendNotification);
            }
            else
            {
                juce::MemoryBlock state;
                if (file.getSize() <= 4 * 1024 * 1024 && file.loadFileAsData(state))
                    safe->processor.setStateInformation(state.getData(), int(state.getSize()));
            }
        });
}
bool PluginEditor::isInterestedInFileDrag(const juce::StringArray &files)
{
    return files.size() == 1 && juce::File(files[0]).hasFileExtension("wav");
}
void PluginEditor::filesDropped(const juce::StringArray &files, int, int)
{
    if (isInterestedInFileDrag(files))
        processor.loadIr(juce::File(files[0]));
}
} // namespace ch
