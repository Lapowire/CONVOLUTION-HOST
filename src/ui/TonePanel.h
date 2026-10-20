#pragma once
#include "CurveEditor.h"
#include "plugin/PluginProcessor.h"
namespace ch
{
class TonePanel final : public juce::Component, private juce::Timer
{
  public:
    TonePanel(PluginProcessor &, CurveMode);
    ~TonePanel() override;
    void resized() override;

  private:
    void timerCallback() override;
    void update();
    void edit(juce::Slider &, int);
    PluginProcessor &processor;
    CurveMode mode;
    CurveEditor curve;
    juce::TextButton a{"A"}, b{"B"}, copy{"COPY"}, undo{"UNDO"}, redo{"REDO"}, reset{"RESET"},
        bypass{"BYPASS"};
    juce::Label status;
    juce::ComboBox band, type;
    juce::ToggleButton enabled{"ON"};
    juce::Slider frequency, level, detail;
    bool updating = false;
};
} // namespace ch
