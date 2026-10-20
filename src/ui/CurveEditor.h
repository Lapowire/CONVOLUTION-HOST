#pragma once
#include "Theme.h"
#include "plugin/ToneController.h"
namespace ch
{
enum class CurveMode
{
    postEq,
    damping
};
class CurveEditor final : public juce::Component
{
  public:
    CurveEditor(ToneController &, CurveMode);
    ~CurveEditor() override;
    void setDisplayState(const ToneState &, const ToneState &, double, bool);
    void setWetFilters(bool, float, float);
    int selectedNode() const noexcept { return selected; }
    void selectNode(int);
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseDoubleClick(const juce::MouseEvent &) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
    bool keyPressed(const juce::KeyPress &) override;
    std::function<void()> onSelection;

  private:
    juce::Rectangle<float> plot() const;
    float xFor(double) const;
    float yFor(double) const;
    float frequencyFor(float) const;
    float valueFor(float) const;
    float frequency(int) const;
    float value(int) const;
    void moveNode(float, float);
    void refresh();
    ToneController &controller;
    CurveMode mode;
    ToneState requested, applied;
    double rate = 48000;
    int selected = 0;
    bool pending = false, dragging = false, wetFilters = false;
    float highpass = 20, lowpass = 20000;
};
} // namespace ch
