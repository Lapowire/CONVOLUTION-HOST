#pragma once
#include "IrDisplay.h"
#include "LibraryPanel.h"
#include "RoutingPanel.h"
#include "Theme.h"
namespace ch
{
class PluginEditor final : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::Timer
{
  public:
    explicit PluginEditor(PluginProcessor &);
    ~PluginEditor() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray &) override;
    void filesDropped(const juce::StringArray &, int, int) override;

  private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    void openIr();
    void preset(bool save);
    void changeParameter(const char *, float);
    PluginProcessor &processor;
    ui::Theme theme;
    LibraryPanel library;
    IrDisplay display;
    RoutingPanel routing;
    juce::TextButton open{"OPEN IR"}, loadPreset{"LOAD"}, savePreset{"SAVE"}, bypass{"BYPASS"},
        decay{"DECAY"};
    juce::Label irName, status, meter, shapeHint;
    juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
    juce::Component eqPage, shapePage;
    std::array<juce::Slider, 6> mainControls;
    std::array<juce::Slider, 10> eqControls;
    std::array<juce::Label, 10> eqLabels;
    std::array<juce::Slider, 4> shapeControls;
    std::array<juce::Label, 4> shapeLabels;
    juce::Slider highpass, lowpass;
    juce::ToggleButton filtersEnabled{"WET FILTERS"}, normalise{"Normalise IR (common gain)"};
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::FileChooser> chooser;
    bool choosing = false;
    juce::Rectangle<int> mainArea;
};
} // namespace ch
