#include "TonePanel.h"
namespace ch
{
TonePanel::TonePanel(PluginProcessor &p, CurveMode m) : processor(p), mode(m), curve(p.toneController(), m)
{
    setComponentID(m == CurveMode::postEq ? "postEqPanel" : "dampingPanel");
    for (auto *c : std::initializer_list<juce::Component *>{&curve, &a, &b, &copy, &undo, &redo, &reset,
                                                            &bypass, &status, &band, &type, &enabled,
                                                            &frequency, &level, &detail})
        addAndMakeVisible(c);
    int count = m == CurveMode::postEq ? 10 : 6;
    for (int i = 1; i <= count; ++i)
        band.addItem(juce::String(i), i);
    band.setTitle("Selected node");
    band.setComponentID(m == CurveMode::postEq ? "eqBand" : "dampingBand");
    band.onChange = [this]
    {
        if (!updating)
            curve.selectNode(band.getSelectedId() - 1);
    };
    type.addItemList({"Bell", "Low shelf", "High shelf", "Notch"}, 1);
    type.setTitle("EQ filter type");
    type.setComponentID("eqType");
    type.onChange = [this]
    {
        if (!updating)
            processor.toneController().setParameter("peq" + juce::String(curve.selectedNode()) + "Type",
                                                    float(type.getSelectedId() - 1));
    };
    enabled.setTitle("Selected EQ band enabled");
    enabled.onClick = [this]
    {
        processor.toneController().setParameter("peq" + juce::String(curve.selectedNode()) + "Enabled",
                                                enabled.getToggleState() ? 1 : 0);
    };
    for (auto *s : {&frequency, &level, &detail})
    {
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 18);
        s->setWantsKeyboardFocus(true);
        s->onDragStart = [this] { processor.toneController().beginGesture(); };
        s->onDragEnd = [this] { processor.toneController().endGesture(); };
    }
    frequency.setTitle("Node frequency");
    frequency.setComponentID(m == CurveMode::postEq ? "eqFrequency" : "dampingFrequency");
    frequency.setTextValueSuffix(" Hz");
    level.setTitle(m == CurveMode::postEq ? "EQ band gain" : "Additional decay attenuation");
    level.setComponentID(m == CurveMode::postEq ? "eqGain" : "dampingRate");
    level.setTextValueSuffix(m == CurveMode::postEq ? " dB" : " dB/s");
    detail.setTitle(m == CurveMode::postEq ? "EQ bandwidth Q" : "Early protection");
    detail.setComponentID(m == CurveMode::postEq ? "eqQ" : "dampingProtection");
    detail.setTextValueSuffix(m == CurveMode::postEq ? " Q" : " ms early");
    frequency.onValueChange = [this] { edit(frequency, 0); };
    level.onValueChange = [this] { edit(level, 1); };
    detail.onValueChange = [this] { edit(detail, 2); };
    a.onClick = [this]
    {
        processor.toneController().selectSlot(0);
        update();
    };
    b.onClick = [this]
    {
        processor.toneController().selectSlot(1);
        update();
    };
    copy.onClick = [this]
    {
        int active = processor.toneController().snapshot().active;
        processor.toneController().copySlot(active, 1 - active);
        update();
    };
    undo.onClick = [this]
    {
        processor.toneController().undo();
        update();
    };
    redo.onClick = [this]
    {
        processor.toneController().redo();
        update();
    };
    reset.onClick = [this]
    {
        if (mode == CurveMode::postEq)
            processor.toneController().resetEq();
        else
            processor.toneController().resetDamping();
        update();
    };
    bypass.setClickingTogglesState(true);
    bypass.onClick = [this]
    {
        processor.toneController().setParameter(mode == CurveMode::postEq ? "eqBypass" : "dampBypass",
                                                bypass.getToggleState() ? 1 : 0);
        update();
    };
    copy.setTooltip("Copy active tone configuration to the other A/B slot");
    status.setFont(ui::font(10));
    status.setJustificationType(juce::Justification::centredRight);
    type.setVisible(m == CurveMode::postEq);
    enabled.setVisible(m == CurveMode::postEq);
    curve.onSelection = [this] { update(); };
    startTimerHz(20);
    update();
}
TonePanel::~TonePanel()
{
    stopTimer();
    processor.toneController().endGesture();
}
void TonePanel::resized()
{
    int x = 4;
    for (auto *button : {&a, &b, &copy, &undo, &redo, &reset, &bypass})
    {
        int width = button == &a || button == &b ? 28 : button == &bypass ? 62 : 48;
        button->setBounds(x, 3, width, 24);
        x += width + 4;
    }
    status.setBounds(x + 4, 3, std::max(0, getWidth() - x - 8), 24);
    curve.setBounds(4, 32, getWidth() - 8, std::max(20, getHeight() - 82));
    int y = getHeight() - 45, w = getWidth() - 8;
    band.setBounds(4, y + 10, 56, 26);
    int usable = w - 68;
    if (mode == CurveMode::postEq)
    {
        int cell = std::max(70, (usable - 158) / 3);
        frequency.setBounds(68, y, cell, 42);
        level.setBounds(68 + cell, y, cell, 42);
        detail.setBounds(68 + 2 * cell, y, cell, 42);
        type.setBounds(68 + 3 * cell, y + 10, 100, 26);
        enabled.setBounds(172 + 3 * cell, y + 10, 48, 26);
    }
    else
    {
        int cell = usable / 3;
        frequency.setBounds(68, y, cell, 42);
        level.setBounds(68 + cell, y, cell, 42);
        detail.setBounds(68 + 2 * cell, y, cell, 42);
    }
}
void TonePanel::edit(juce::Slider &slider, int kind)
{
    if (updating)
        return;
    int index = curve.selectedNode();
    auto &controller = processor.toneController();
    juce::String id;
    if (mode == CurveMode::postEq)
        id =
            kind == 1 ? "eq" + juce::String(index) : "peq" + juce::String(index) + (kind == 0 ? "Freq" : "Q");
    else
        id = kind == 2 ? "dampProtect" : "damp" + juce::String(index) + (kind == 0 ? "Freq" : "Rate");
    controller.setParameter(id, float(slider.getValue()));
}
void TonePanel::timerCallback()
{
    update();
}
void TonePanel::update()
{
    updating = true;
    auto &controller = processor.toneController();
    auto state = controller.current();
    auto memory = controller.snapshot();
    auto loader = processor.reverb.loader().status();
    double rate = processor.getSampleRate() > 0 ? processor.getSampleRate() : 48000;
    auto active = state;
    active.damping = loader.activeDamping;
    bool pending = state.damping != active.damping || loader.loading || loader.activeId == 0;
    curve.setWetFilters(processor.parameters.getRawParameterValue("filtersEnabled")->load() >= 0.5f,
                        processor.parameters.getRawParameterValue("highpass")->load(),
                        processor.parameters.getRawParameterValue("lowpass")->load());
    curve.setDisplayState(state, active, rate, pending);
    int i = curve.selectedNode();
    auto n = std::size_t(i);
    band.setSelectedId(i + 1, juce::dontSendNotification);
    a.setToggleState(memory.active == 0, juce::dontSendNotification);
    b.setToggleState(memory.active == 1, juce::dontSendNotification);
    undo.setEnabled(controller.canUndo());
    redo.setEnabled(controller.canRedo());
    bypass.setToggleState(mode == CurveMode::postEq ? state.eq.bypass : state.damping.bypass,
                          juce::dontSendNotification);
    if (mode == CurveMode::postEq)
    {
        frequency.setRange(
            20, state.eq.bands[n].frequencyHz < rate * 0.5 ? std::min(20000., rate * 0.499) : 20000., 0.1);
        frequency.setSkewFactorFromMidPoint(632.4555);
        frequency.setValue(state.eq.bands[n].frequencyHz, juce::dontSendNotification);
        level.setRange(-12, 12, 0.01);
        level.setValue(state.eq.gainDb[n], juce::dontSendNotification);
        detail.setRange(0.1, 18, 0.01);
        detail.setSkewFactorFromMidPoint(1.3416);
        detail.setValue(state.eq.bands[n].q, juce::dontSendNotification);
        type.setSelectedId(int(state.eq.bands[n].type) + 1, juce::dontSendNotification);
        enabled.setToggleState(state.eq.bands[n].enabled, juce::dontSendNotification);
        bool legal = state.eq.bands[n].frequencyHz < rate * 0.5;
        frequency.setEnabled(legal);
        level.setEnabled(legal && state.eq.bands[n].type != EqType::notch);
        detail.setEnabled(legal);
        type.setEnabled(legal);
        enabled.setEnabled(legal);
        status.setText(legal ? "POST EQ / Hz · dB · Q" : "ABOVE NYQUIST", juce::dontSendNotification);
    }
    else
    {
        double lo = i ? state.damping.nodes[n - 1].frequencyHz + 0.2 : 20,
               hi = i < 5 ? state.damping.nodes[n + 1].frequencyHz - 0.2 : 20000;
        bool legal = lo < hi && state.damping.nodes[n].frequencyHz < rate * 0.5;
        if (legal)
            hi = std::min(hi, rate * 0.499);
        frequency.setRange(lo < hi ? lo : 20, lo < hi ? hi : 20000, 0.1);
        frequency.setValue(state.damping.nodes[n].frequencyHz, juce::dontSendNotification);
        frequency.setEnabled(legal);
        level.setRange(0, 60, 0.01);
        level.setValue(state.damping.nodes[n].attenuationDbPerSecond, juce::dontSendNotification);
        level.setEnabled(legal);
        detail.setRange(0, 200, 0.1);
        detail.setValue(state.damping.protectMs, juce::dontSendNotification);
        status.setText(loader.error.isNotEmpty() ? "ERROR / PREVIOUS ACTIVE"
                       : !loader.ir              ? "SELECT AN IR"
                       : pending                 ? "PENDING / GREY = ACTIVE"
                                                 : "APPLIED / dB PER SECOND",
                       juce::dontSendNotification);
    }
    status.setColour(juce::Label::textColourId, mode == CurveMode::damping ? ui::amber : ui::muted);
    updating = false;
}
} // namespace ch
