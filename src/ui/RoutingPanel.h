#pragma once
#include "MatrixEditor.h"
#include "Theme.h"
#include "plugin/PluginProcessor.h"
namespace ch
{
class RoutingPanel : public juce::Component, private juce::Timer
{
  public:
    explicit RoutingPanel(PluginProcessor &);
    ~RoutingPanel() override { stopTimer(); }
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    void chooseSource(int);
    void refreshFromState();
    void timerCallback() override
    {
        if (shownRevision != processor.selectionVersion())
            refreshFromState();
    }
    std::uint64_t shownRevision = 0;
    std::array<float, 4> polarities{1, 1, 1, 1};
    void updateMode();
    int rowHeight() const { return std::min(24, std::max(18, (getHeight() - 60) / 4)); }
    PluginProcessor &processor;
    juce::ComboBox mode, interpretation;
    juce::ToggleButton lfeWet{"LFE wet (explicit)"};
    MatrixEditor matrix;
    std::array<juce::ComboBox, 4> channels;
    std::array<juce::Slider, 4> gains;
    std::array<juce::TextButton, 4> sourceButtons;
    std::array<juce::File, 4> sourceFiles;
    juce::TextButton apply{"Apply routing"};
    std::unique_ptr<juce::FileChooser> chooser;
    bool choosing = false;
};
} // namespace ch
