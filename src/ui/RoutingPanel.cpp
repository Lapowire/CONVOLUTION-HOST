#include "RoutingPanel.h"
namespace ch
{
RoutingPanel::RoutingPanel(PluginProcessor &p) : processor(p)
{
    mode.addItem("Mono / stereo diagonal", 1);
    mode.addItem("True stereo: multi-channel WAV", 2);
    mode.addItem("True stereo: four mono WAV files", 3);
    mode.setComponentID("routingMode");
    addAndMakeVisible(mode);
    addAndMakeVisible(apply);
    auto state = processor.selectedState();
    mode.setSelectedId(state.request.additionalFiles.size() == 3 ? 3
                       : state.routing.routes.empty()            ? 1
                                                                 : 2,
                       juce::dontSendNotification);
    if (state.request.additionalFiles.size() == 3)
    {
        sourceFiles[0] = state.request.file;
        for (int i = 1; i < 4; ++i)
            sourceFiles[std::size_t(i)] = state.request.additionalFiles[std::size_t(i - 1)];
    }
    for (int i = 0; i < 4; ++i)
    {
        const auto n = std::size_t(i);
        auto &c = channels[n];
        c.addItem("Off", 1);
        for (int channel = 1; channel <= 16; ++channel)
            c.addItem("IR channel " + juce::String(channel), channel + 1);
        c.setSelectedId(i + 2, juce::dontSendNotification);
        c.setTitle("IR channel for path " + juce::String(i + 1));
        addAndMakeVisible(c);
        auto &g = gains[n];
        g.setRange(-60, 12, 0.1);
        g.setValue(0);
        g.setSliderStyle(juce::Slider::LinearHorizontal);
        g.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
        g.setTextValueSuffix(" dB");
        addAndMakeVisible(g);
        addAndMakeVisible(sourceButtons[n]);
        sourceButtons[n].setButtonText(sourceFiles[n] == juce::File{} ? "Choose WAV"
                                                                      : sourceFiles[n].getFileName());
        sourceButtons[n].onClick = [this, i] { chooseSource(i); };
        if (n < state.routing.routes.size())
        {
            c.setSelectedId(state.routing.routes[n].irChannel + 2, juce::dontSendNotification);
            g.setValue(juce::Decibels::gainToDecibels(state.routing.routes[n].gain),
                       juce::dontSendNotification);
        }
    }
    mode.onChange = [this] { updateMode(); };
    updateMode();
    apply.onClick = [this]
    {
        RoutingModel model;
        const int selected = mode.getSelectedId();
        if (selected != 1)
        {
            for (int i = 0; i < 4; ++i)
            {
                const int c = selected == 3 ? i : channels[std::size_t(i)].getSelectedId() - 2;
                if (c >= 0)
                    model.routes.push_back(
                        {i / 2, i % 2, c,
                         juce::Decibels::decibelsToGain(float(gains[std::size_t(i)].getValue()))});
            }
            if (model.routes.empty())
                model.routes.push_back({0, 0, 0, 0});
        }
        if (selected == 3)
        {
            for (auto f : sourceFiles)
                if (!f.existsAsFile())
                {
                    apply.setButtonText("Choose all 4 WAVs");
                    return;
                }
            processor.loadTrueStereo(sourceFiles, std::move(model));
        }
        else
            processor.loadIr(processor.selectedState().request.file, std::move(model));
        apply.setButtonText("Apply routing");
    };
}
void RoutingPanel::updateMode()
{
    bool files = mode.getSelectedId() == 3;
    for (std::size_t i = 0; i < 4; ++i)
    {
        channels[i].setEnabled(mode.getSelectedId() == 2);
        sourceButtons[i].setEnabled(files);
        gains[i].setEnabled(mode.getSelectedId() != 1);
    }
}
void RoutingPanel::chooseSource(int i)
{
    if (choosing)
        return;
    choosing = true;
    chooser =
        std::make_unique<juce::FileChooser>("Choose mono WAV for this path", juce::File{}, "*.wav", true);
    auto safe = juce::Component::SafePointer<RoutingPanel>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safe, i](const juce::FileChooser &fc)
                         {
                             if (!safe)
                                 return;
                             safe->choosing = false;
                             auto file = fc.getResult();
                             if (file.existsAsFile())
                             {
                                 safe->sourceFiles[std::size_t(i)] = file;
                                 safe->sourceButtons[std::size_t(i)].setButtonText(file.getFileName());
                                 safe->sourceButtons[std::size_t(i)].setTooltip(file.getFullPathName());
                             }
                         });
}
void RoutingPanel::paint(juce::Graphics &g)
{
    g.setColour(ui::muted);
    g.setFont(ui::font(11));
    g.drawText("Explicit LL / LR / RL / RR paths. Source WAVs stay unchanged.", 16, 5, getWidth() - 32, 18,
               juce::Justification::left);
    const std::array<const char *, 4> names{"L > L", "L > R", "R > L", "R > R"};
    for (int i = 0; i < 4; ++i)
    {
        g.setColour(ui::text);
        g.drawText(names[std::size_t(i)], 16, 60 + i * rowHeight(), 58, rowHeight(),
                   juce::Justification::left);
    }
}
void RoutingPanel::resized()
{
    mode.setBounds(16, 28, getWidth() - 200, 26);
    apply.setBounds(getWidth() - 174, 28, 158, 26);
    for (int i = 0; i < 4; ++i)
    {
        const int y = 60 + i * rowHeight(), h = std::min(22, rowHeight());
        channels[std::size_t(i)].setBounds(76, y, 132, h);
        sourceButtons[std::size_t(i)].setBounds(216, y, 150, h);
        gains[std::size_t(i)].setBounds(378, y, std::max(80, getWidth() - 396), h);
    }
}
} // namespace ch
