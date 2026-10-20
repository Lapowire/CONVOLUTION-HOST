#include "CurveEditor.h"
#include <complex>
namespace ch
{
CurveEditor::CurveEditor(ToneController &c, CurveMode m) : controller(c), mode(m)
{
    setWantsKeyboardFocus(true);
    setTitle(m == CurveMode::postEq ? "Parametric equalizer curve" : "Frequency dependent decay curve");
    setDescription("Arrow keys edit selected node; Tab focuses numeric controls; drag frequency and level; "
                   "wheel changes Q");
    setComponentID(m == CurveMode::postEq ? "eqCurve" : "dampingCurve");
    requested = applied = c.current();
}
CurveEditor::~CurveEditor()
{
    if (dragging)
        controller.endGesture();
}
juce::Rectangle<float> CurveEditor::plot() const
{
    return getLocalBounds().toFloat().reduced(42, 10).withTrimmedBottom(17);
}
float CurveEditor::xFor(double f) const
{
    auto r = plot();
    return r.getX() + float(std::log(juce::jlimit(20., 20000., f) / 20) / std::log(1000.)) * r.getWidth();
}
float CurveEditor::yFor(double v) const
{
    auto r = plot();
    return mode == CurveMode::postEq ? r.getCentreY() - float(v / 36) * r.getHeight()
                                     : r.getBottom() - float(v / 60) * r.getHeight();
}
float CurveEditor::frequencyFor(float x) const
{
    auto r = plot();
    return float(20 * std::pow(1000., juce::jlimit(0.f, 1.f, (x - r.getX()) / std::max(1.f, r.getWidth()))));
}
float CurveEditor::valueFor(float y) const
{
    auto r = plot();
    return mode == CurveMode::postEq
               ? juce::jlimit(-12.f, 12.f, (r.getCentreY() - y) / std::max(1.f, r.getHeight()) * 36)
               : juce::jlimit(0.f, 60.f, (r.getBottom() - y) / std::max(1.f, r.getHeight()) * 60);
}
float CurveEditor::frequency(int i) const
{
    return mode == CurveMode::postEq ? requested.eq.bands[std::size_t(i)].frequencyHz
                                     : requested.damping.nodes[std::size_t(i)].frequencyHz;
}
float CurveEditor::value(int i) const
{
    return mode == CurveMode::postEq ? (requested.eq.bands[std::size_t(i)].type == EqType::notch
                                            ? -12.f
                                            : requested.eq.gainDb[std::size_t(i)])
                                     : requested.damping.nodes[std::size_t(i)].attenuationDbPerSecond;
}
void CurveEditor::setDisplayState(const ToneState &r, const ToneState &a, double sr, bool p)
{
    requested = r;
    applied = a;
    rate = sr > 0 ? sr : 48000;
    pending = p;
    repaint();
}
void CurveEditor::setWetFilters(bool enabled, float hp, float lp)
{
    wetFilters = enabled;
    highpass = hp;
    lowpass = lp;
}
void CurveEditor::selectNode(int i)
{
    selected = juce::jlimit(0, mode == CurveMode::postEq ? 9 : 5, i);
    if (onSelection)
        onSelection();
    repaint();
}
void CurveEditor::paint(juce::Graphics &g)
{
    auto r = plot();
    if (r.getWidth() < 1 || r.getHeight() < 1)
        return;
    g.setColour(ui::background);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6);
    g.setFont(ui::font(10));
    for (float f : {20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f})
    {
        float x = xFor(f);
        g.setColour(ui::line);
        g.drawVerticalLine(int(x), r.getY(), r.getBottom());
        g.setColour(ui::muted);
        g.drawText(f >= 1000 ? juce::String(f / 1000, 0) + "k" : juce::String(f, 0), int(x) - 17,
                   int(r.getBottom()) + 2, 34, 13, juce::Justification::centred);
    }
    for (int v : mode == CurveMode::postEq ? std::array<int, 5>{-12, -6, 0, 6, 12}
                                           : std::array<int, 5>{0, 15, 30, 45, 60})
    {
        float y = yFor(v);
        g.setColour(ui::line);
        g.drawHorizontalLine(int(y), r.getX(), r.getRight());
        g.setColour(ui::muted);
        g.drawText(juce::String(v), 1, int(y) - 6, 34, 13, juce::Justification::centredRight);
    }
    if (rate * 0.5 < 20000)
    {
        g.setColour(ui::muted.withAlpha(0.08f));
        g.fillRect(juce::Rectangle<float>(xFor(rate * 0.5), r.getY(), r.getRight() - xFor(rate * 0.5),
                                          r.getHeight()));
    }
    auto colour = mode == CurveMode::postEq ? ui::accent : ui::amber;
    auto drawCurve = [&](const ToneState &state, juce::Colour c, bool individual)
    {
        juce::Path path;
        auto eq = state.eq;
        if (individual)
            for (int j = 0; j < 10; ++j)
                if (j != selected)
                    eq.bands[std::size_t(j)].enabled = false;
        for (int x = 0; x <= int(r.getWidth()); ++x)
        {
            double f = frequencyFor(r.getX() + x), v = 0;
            if (mode == CurveMode::postEq)
            {
                v = 20 * std::log10(std::max(1e-9, ParametricEq::magnitude(eq, rate, f)));
                if (wetFilters && !individual)
                {
                    // Same second-order Butterworth coefficients as the wet HP/LP path.
                    for (auto pair : {std::pair<double, bool>{highpass, true}, {lowpass, false}})
                    {
                        double w = 2 * juce::MathConstants<double>::pi * std::min(pair.first, rate * 0.45) /
                                   rate,
                               cosine = std::cos(w), alpha = std::sin(w) / (2 * 0.7071067811865476),
                               a0 = 1 + alpha, b0 = (pair.second ? 1 + cosine : 1 - cosine) * 0.5;
                        auto z = std::polar(1., -2 * juce::MathConstants<double>::pi * f / rate);
                        auto response = (b0 + (pair.second ? -2 : 2) * b0 * z + b0 * z * z) /
                                        (a0 - 2 * cosine * z + (1 - alpha) * z * z);
                        v += 20 * std::log10(std::max(1e-9, std::abs(response)));
                    }
                }
            }
            else
                v = state.damping.bypass ? 0 : state.damping.attenuationAt(f);
            float y = juce::jlimit(r.getY(), r.getBottom(), yFor(v));
            if (x == 0)
                path.startNewSubPath(r.getX(), y);
            else
                path.lineTo(r.getX() + x, y);
        }
        g.setColour(c);
        g.strokePath(path, juce::PathStrokeType(individual ? 1.f : 2.f));
    };
    {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(r.toNearestInt());
        if (mode == CurveMode::postEq)
            drawCurve(requested, colour.withAlpha(0.25f), true);
        if (pending && mode == CurveMode::damping)
            drawCurve(applied, ui::muted.withAlpha(0.8f), false);
        drawCurve(requested, colour.withAlpha(pending ? 0.65f : 1.f), false);
    }
    for (int i = 0; i < (mode == CurveMode::postEq ? 10 : 6); ++i)
    {
        float x = xFor(frequency(i)), y = yFor(value(i));
        bool enabled = frequency(i) < rate * 0.5 &&
                       (mode == CurveMode::damping || requested.eq.bands[std::size_t(i)].enabled);
        g.setColour(enabled ? colour : ui::muted);
        g.fillEllipse(x - 4, y - 4, 8, 8);
        if (i == selected)
        {
            g.drawEllipse(x - 7, y - 7, 14, 14, 1.2f);
            g.setFont(ui::font(10, true));
            g.drawText(juce::String(i + 1), int(x) - 7, int(y) - 22, 20, 14, juce::Justification::centred);
        }
    }
}
void CurveEditor::refresh()
{
    requested = controller.current();
    if (onSelection)
        onSelection();
    repaint();
}
void CurveEditor::moveNode(float f, float v)
{
    if (frequency(selected) >= rate * 0.5)
        return;
    f = juce::jlimit(20.f, float(std::min(20000., rate * 0.499)), f);
    if (mode == CurveMode::postEq)
    {
        auto prefix = "peq" + juce::String(selected);
        controller.setParameter(prefix + "Freq", f);
        if (requested.eq.bands[std::size_t(selected)].type != EqType::notch)
            controller.setParameter("eq" + juce::String(selected), v);
    }
    else
    {
        float lo = selected ? frequency(selected - 1) + 0.2f : 20.f,
              hi = selected < 5 ? frequency(selected + 1) - 0.2f : float(std::min(20000., rate * 0.499));
        if (lo <= hi)
            controller.setParameter("damp" + juce::String(selected) + "Freq", juce::jlimit(lo, hi, f));
        controller.setParameter("damp" + juce::String(selected) + "Rate", juce::jlimit(0.f, 60.f, v));
    }
    refresh();
}
void CurveEditor::mouseDown(const juce::MouseEvent &e)
{
    grabKeyboardFocus();
    float distance = 1e9f;
    int found = selected;
    for (int i = 0; i < (mode == CurveMode::postEq ? 10 : 6); ++i)
    {
        float d = e.position.getDistanceFrom({xFor(frequency(i)), yFor(value(i))});
        if (d < distance)
        {
            distance = d;
            found = i;
        }
    }
    selectNode(found);
    if (distance < 22 && frequency(selected) < rate * 0.5)
    {
        controller.beginGesture();
        dragging = true;
    }
}
void CurveEditor::mouseDrag(const juce::MouseEvent &e)
{
    if (dragging)
        moveNode(frequencyFor(e.position.x), valueFor(e.position.y));
}
void CurveEditor::mouseUp(const juce::MouseEvent &)
{
    if (dragging)
    {
        controller.endGesture();
        dragging = false;
    }
}
void CurveEditor::mouseDoubleClick(const juce::MouseEvent &)
{
    controller.setParameter((mode == CurveMode::postEq ? "eq" : "damp") + juce::String(selected) +
                                (mode == CurveMode::damping ? "Rate" : ""),
                            0);
    refresh();
}
void CurveEditor::mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &w)
{
    if (mode == CurveMode::postEq && frequency(selected) < rate * 0.5)
    {
        controller.setParameter("peq" + juce::String(selected) + "Q",
                                requested.eq.bands[std::size_t(selected)].q * float(std::exp(w.deltaY * 2)));
        refresh();
    }
}
bool CurveEditor::keyPressed(const juce::KeyPress &key)
{
    int k = key.getKeyCode();
    if (k == juce::KeyPress::pageUpKey || k == juce::KeyPress::pageDownKey)
    {
        selectNode(selected + (k == juce::KeyPress::pageUpKey ? -1 : 1));
        return true;
    }
    float f = frequency(selected), v = value(selected);
    if (k == juce::KeyPress::leftKey)
        f /= 1.02f;
    else if (k == juce::KeyPress::rightKey)
        f *= 1.02f;
    else if (k == juce::KeyPress::upKey)
        v += mode == CurveMode::postEq ? 0.5f : 1.f;
    else if (k == juce::KeyPress::downKey)
        v -= mode == CurveMode::postEq ? 0.5f : 1.f;
    else
        return false;
    controller.beginGesture();
    moveNode(f, v);
    controller.endGesture();
    return true;
}
} // namespace ch
