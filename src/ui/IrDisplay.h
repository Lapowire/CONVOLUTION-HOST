#pragma once
#include "Theme.h"
#include "ir/PreparedIr.h"
namespace ch
{
class IrDisplay : public juce::Component
{
  public:
    void setIr(std::shared_ptr<const PreparedIr> p)
    {
        if (impulse != p)
        {
            impulse = std::move(p);
            repaint();
        }
    }
    void setDecay(bool enabled)
    {
        decay = enabled;
        repaint();
    }
    void setTrim(float start, float end)
    {
        trimStart = start;
        trimEnd = end;
        repaint();
    }
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    std::function<void(float, float)> onSelection;

  private:
    juce::Rectangle<float> plot() const
    {
        return getLocalBounds().toFloat().reduced(20).withTrimmedTop(24).withTrimmedBottom(16);
    }
    std::shared_ptr<const PreparedIr> impulse;
    bool decay = false;
    float trimStart = 0, trimEnd = 0, dragStart = 0;
};
} // namespace ch
