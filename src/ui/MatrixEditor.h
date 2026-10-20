#pragma once
#include "Theme.h"
#include "audio/RoutingModel.h"
namespace ch
{
class MatrixEditor : public juce::Component
{
  public:
    MatrixEditor();
    ~MatrixEditor() override;
    void setModel(const BusLayout &, const RoutingModel &);
    RoutingModel model() const;
    void resized() override;

  private:
    struct Row;
    void append(Route);
    void arrange();
    BusLayout layout;
    RoutingModel base;
    juce::Component contents;
    juce::Viewport viewport;
    juce::TextButton add{"+ Path"}, remove{"Remove last"};
    juce::Label hint;
    std::vector<std::unique_ptr<Row>> rows;
};
} // namespace ch
