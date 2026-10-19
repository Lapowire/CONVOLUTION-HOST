#pragma once
#include "Theme.h"
#include "plugin/PluginProcessor.h"
namespace ch
{
class RoutingPanel : public juce::Component
{
  public:
    explicit RoutingPanel(PluginProcessor &);
    void paint(juce::Graphics &) override;
    void resized() override;

  private:
    void chooseSource(int);
    void updateMode();
    int rowHeight() const { return std::min(24, std::max(18, (getHeight() - 60) / 4)); }
    PluginProcessor &processor;
    juce::ComboBox mode;
    std::array<juce::ComboBox, 4> channels;
    std::array<juce::Slider, 4> gains;
    std::array<juce::TextButton, 4> sourceButtons;
    std::array<juce::File, 4> sourceFiles;
    juce::TextButton apply{"Apply routing"};
    std::unique_ptr<juce::FileChooser> chooser;
    bool choosing = false;
};
} // namespace ch
