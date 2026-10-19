#include "IrDisplay.h"
namespace ch
{
void IrDisplay::paint(juce::Graphics &g)
{
    ui::card(g, getLocalBounds().toFloat());
    g.setColour(ui::muted);
    g.setFont(ui::font(11, true));
    g.drawText(decay ? "ENERGY DECAY / SCHROEDER" : "IMPULSE RESPONSE / WAVEFORM", 20, 12, getWidth() - 40,
               20, juce::Justification::centredLeft);
    const auto r = plot();
    g.setColour(ui::line);
    for (int i = 0; i <= 8; ++i)
    {
        float x = r.getX() + r.getWidth() * i / 8;
        g.drawVerticalLine(int(x), r.getY(), r.getBottom());
    }
    if (!impulse)
    {
        g.setColour(ui::text);
        g.setFont(ui::font(18));
        g.drawText("Your next space starts here", r.toNearestInt(), juce::Justification::centred);
        g.setColour(ui::muted);
        g.setFont(ui::font(12));
        g.drawText("Drop a WAV impulse response or open your library", 20, getHeight() - 38, getWidth() - 40,
                   20, juce::Justification::centred);
        return;
    }
    if (decay)
    {
        for (int d = 0; d <= 4; ++d)
        {
            float y = r.getY() + r.getHeight() * d / 4;
            g.setColour(ui::line);
            g.drawHorizontalLine(int(y), r.getX(), r.getRight());
            g.setColour(ui::muted);
            g.setFont(ui::font(10));
            g.drawText(juce::String(-30 * d) + " dB", int(r.getX() + 3), int(y), 45, 14,
                       juce::Justification::left);
        }
        juce::Path p;
        for (std::size_t i = 0; i < impulse->decayDb.size(); ++i)
        {
            float x = r.getX() +
                      r.getWidth() * float(i) / float(std::max(std::size_t(1), impulse->decayDb.size() - 1));
            float y = r.getY() + r.getHeight() * juce::jlimit(0.f, 1.f, -impulse->decayDb[i] / 120);
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
        }
        g.setColour(ui::accent);
        g.strokePath(p, juce::PathStrokeType(2));
    }
    else
    {
        const int channels = impulse->samples.getNumChannels();
        for (int c = 0; c < channels; ++c)
        {
            auto lane = r.withY(r.getY() + r.getHeight() * c / channels).withHeight(r.getHeight() / channels);
            const auto &bins = impulse->channelWaveforms.empty() ? impulse->waveform
                                                                 : impulse->channelWaveforms[std::size_t(c)];
            g.setColour(ui::line);
            g.drawHorizontalLine(int(lane.getCentreY()), lane.getX(), lane.getRight());
            juce::Path p;
            for (std::size_t i = 0; i < bins.size(); ++i)
            {
                float x = lane.getX() +
                          lane.getWidth() * float(i) / float(std::max(std::size_t(1), bins.size() - 1)),
                      v = std::min(1.f, bins[i]) * lane.getHeight() * 0.43f;
                if (i == 0)
                    p.startNewSubPath(x, lane.getCentreY() - v);
                else
                    p.lineTo(x, lane.getCentreY() - v);
            }
            for (std::size_t i = bins.size(); i-- > 0;)
            {
                float x = lane.getX() +
                          lane.getWidth() * float(i) / float(std::max(std::size_t(1), bins.size() - 1));
                p.lineTo(x, lane.getCentreY() + std::min(1.f, bins[i]) * lane.getHeight() * 0.43f);
            }
            p.closeSubPath();
            g.setColour(ui::accent.withAlpha(0.7f));
            g.fillPath(p);
        }
    }
    const float duration = float(impulse->durationSeconds() * 1000),
                a = juce::jlimit(0.f, 1.f, trimStart / duration),
                b = trimEnd > 0 ? juce::jlimit(a, 1.f, trimEnd / duration) : 1.f;
    g.setColour(ui::background.withAlpha(0.7f));
    g.fillRect(r.withWidth(r.getWidth() * a));
    g.fillRect(r.withLeft(r.getX() + r.getWidth() * b));
    g.setColour(ui::amber);
    g.drawVerticalLine(int(r.getX() + r.getWidth() * a), r.getY(), r.getBottom());
    g.drawVerticalLine(int(r.getX() + r.getWidth() * b) - 1, r.getY(), r.getBottom());
    g.setColour(ui::muted);
    g.setFont(ui::font(10));
    g.drawText("0 ms", 20, getHeight() - 22, 80, 16, juce::Justification::left);
    g.drawText(juce::String(duration, 1) + " ms", getWidth() - 120, getHeight() - 22, 100, 16,
               juce::Justification::right);
}
void IrDisplay::mouseDown(const juce::MouseEvent &e)
{
    dragStart = juce::jlimit(0.f, 1.f, (e.position.x - plot().getX()) / plot().getWidth());
}
void IrDisplay::mouseUp(const juce::MouseEvent &e)
{
    if (!impulse || !onSelection)
        return;
    float end = juce::jlimit(0.f, 1.f, (e.position.x - plot().getX()) / plot().getWidth());
    if (std::abs(end - dragStart) < 0.005f)
        return;
    float duration = float(impulse->durationSeconds() * 1000);
    onSelection(std::min(dragStart, end) * duration, std::max(dragStart, end) * duration);
}
} // namespace ch
