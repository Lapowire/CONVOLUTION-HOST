#include "MatrixEditor.h"
namespace ch
{
struct MatrixEditor::Row : juce::Component
{
    juce::ComboBox input, output, ir;
    juce::Slider gain;
    juce::ToggleButton inverted{"Invert"};
    Row(const BusLayout &bus, Route route, int index)
    {
        for (int i = 0; i < bus.labels.size(); ++i)
        {
            input.addItem(bus.labels[i], i + 1);
            output.addItem(bus.labels[i], i + 1);
        }
        for (int i = 0; i < 16; ++i)
            ir.addItem("IR " + juce::String(i + 1), i + 1);
        input.setSelectedId(route.input + 1);
        output.setSelectedId(route.output + 1);
        ir.setSelectedId(route.irChannel + 1);
        input.setComponentID("route" + juce::String(index) + ".input");
        output.setComponentID("route" + juce::String(index) + ".output");
        ir.setComponentID("route" + juce::String(index) + ".ir");
        input.setTitle("Input channel");
        output.setTitle("Output channel");
        ir.setTitle("Impulse channel");
        gain.setRange(0, 16, 0.001);
        gain.setValue(std::abs(route.gain));
        gain.setTextValueSuffix(" x");
        gain.setSliderStyle(juce::Slider::LinearHorizontal);
        gain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 22);
        gain.setTitle("Path gain");
        inverted.setToggleState(route.gain < 0, juce::dontSendNotification);
        inverted.setTitle("Invert path polarity");
        for (auto *c : std::initializer_list<juce::Component *>{&input, &output, &ir, &gain, &inverted})
            addAndMakeVisible(c);
    }
    Route route() const
    {
        return {input.getSelectedId() - 1, output.getSelectedId() - 1, ir.getSelectedId() - 1,
                float(gain.getValue()) * (inverted.getToggleState() ? -1.f : 1.f)};
    }
    void resized() override
    {
        int col = (getWidth() - 260) / 3;
        input.setBounds(0, 2, col, 26);
        output.setBounds(col + 4, 2, col, 26);
        ir.setBounds(2 * col + 8, 2, col, 26);
        gain.setBounds(3 * col + 12, 2, 160, 26);
        inverted.setBounds(getWidth() - 84, 2, 84, 26);
    }
};
MatrixEditor::MatrixEditor()
{
    viewport.setViewedComponent(&contents, false);
    viewport.setComponentID("matrixViewport");
    for (auto *c : std::initializer_list<juce::Component *>{&viewport, &add, &remove, &hint})
        addAndMakeVisible(c);
    add.setComponentID("matrixAdd");
    remove.setComponentID("matrixRemove");
    hint.setFont(ui::font(10));
    hint.setText("Input > Output / IR channel / gain", juce::dontSendNotification);
    add.onClick = [this]
    {
        if (rows.size() < 256)
            append({});
    };
    remove.onClick = [this]
    {
        if (!rows.empty())
        {
            rows.pop_back();
            arrange();
        }
    };
}
MatrixEditor::~MatrixEditor() = default;
void MatrixEditor::append(Route route)
{
    rows.push_back(std::make_unique<Row>(layout, route, int(rows.size())));
    contents.addAndMakeVisible(*rows.back());
    arrange();
}
void MatrixEditor::setModel(const BusLayout &bus, const RoutingModel &value)
{
    layout = bus;
    base = value;
    rows.clear();
    for (auto route : value.routes)
        append(route);
    arrange();
}
RoutingModel MatrixEditor::model() const
{
    auto result = base;
    result.automatic = false;
    result.sourceLayout = layout.identity();
    result.routes.clear();
    for (auto &row : rows)
        result.routes.push_back(row->route());
    return result;
}
void MatrixEditor::arrange()
{
    int width = std::max(540, viewport.getWidth() - 18);
    contents.setSize(width, std::max(1, int(rows.size()) * 32));
    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(0, int(i) * 32, width, 32);
    add.setEnabled(rows.size() < 256);
    remove.setEnabled(!rows.empty());
}
void MatrixEditor::resized()
{
    add.setBounds(0, 0, 82, 24);
    remove.setBounds(88, 0, 106, 24);
    hint.setBounds(204, 0, std::max(0, getWidth() - 204), 24);
    viewport.setBounds(0, 28, getWidth(), std::max(0, getHeight() - 28));
    arrange();
}
} // namespace ch
