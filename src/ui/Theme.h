#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace ch::ui
{
inline const juce::Colour background{0xff101619}, panel{0xff192226}, line{0xff2b393e}, muted{0xff8ca0a6},
    text{0xffe7eeee}, accent{0xff63e3ca}, amber{0xffeeb86d};
inline juce::Font font(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
class Theme : public juce::LookAndFeel_V4
{
  public:
    Theme()
    {
        setColour(juce::Slider::textBoxTextColourId, text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, background);
        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accent);
        setColour(juce::Slider::backgroundColourId, line);
        setColour(juce::TextEditor::backgroundColourId, background);
        setColour(juce::TextEditor::textColourId, text);
        setColour(juce::TextEditor::outlineColourId, line);
        setColour(juce::ComboBox::backgroundColourId, background);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::ListBox::backgroundColourId, panel);
        setColour(juce::Label::textColourId, text);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, background);
    }
    void drawRotarySlider(juce::Graphics &g, int x, int y, int w, int h, float value, float start, float end,
                          juce::Slider &slider) override
    {
        auto bounds = juce::Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(8);
        float size = std::min(bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre(size, size).reduced(3);
        g.setColour(background);
        g.fillEllipse(r);
        g.setColour(line);
        g.drawEllipse(r, 1);
        auto arc = r.reduced(4);
        juce::Path base, active;
        base.addCentredArc(arc.getCentreX(), arc.getCentreY(), arc.getWidth() / 2, arc.getHeight() / 2, 0,
                           start, end, true);
        active.addCentredArc(arc.getCentreX(), arc.getCentreY(), arc.getWidth() / 2, arc.getHeight() / 2, 0,
                             start, start + value * (end - start), true);
        g.setColour(line);
        g.strokePath(base, juce::PathStrokeType(3));
        g.setColour(slider.isEnabled() ? accent : muted);
        g.strokePath(active, juce::PathStrokeType(3));
        const float angle = start + value * (end - start);
        juce::Path pointer;
        pointer.addRoundedRectangle(-1.5f, -size * 0.28f, 3, size * 0.13f, 1.5f);
        pointer.applyTransform(
            juce::AffineTransform::rotation(angle).translated(r.getCentreX(), r.getCentreY()));
        g.fillPath(pointer);
    }
    void drawButtonBackground(juce::Graphics &g, juce::Button &b, const juce::Colour &, bool over,
                              bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(b.getToggleState() ? accent : over ? line : panel);
        g.fillRoundedRectangle(r, 5);
        g.setColour(down ? accent : line);
        g.drawRoundedRectangle(r, 5, 1);
    }
};
inline void card(juce::Graphics &g, juce::Rectangle<float> r)
{
    g.setColour(panel);
    g.fillRoundedRectangle(r, 10);
    g.setColour(line);
    g.drawRoundedRectangle(r.reduced(0.5f), 10, 1);
}
} // namespace ch::ui
