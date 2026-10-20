#pragma once
#include "Parameters.h"
#include "ToneController.h"
#include "ir/IrLibrary.h"
#include "state/StateStore.h"
namespace ch
{
class PluginProcessor final : public juce::AudioProcessor,
                              private juce::Timer,
                              private juce::AudioProcessorValueTreeState::Listener
{
  public:
    explicit PluginProcessor(
        juce::File preferencesFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                         .getChildFile("ConvolutionHost/library.json"));
    ~PluginProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    const juce::String getName() const override { return "CONVOLUTION HOST"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String &) override {}
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor *createEditor() override;
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    void loadIr(const juce::File &, RoutingModel = {});
    void loadTrueStereo(const std::array<juce::File, 4> &, RoutingModel = {});
    void setRouting(RoutingModel);
    std::optional<BusLayout> busLayout() const;
    void setBusInterpretation(BusInterpretation);
    void setLfeWetEnabled(bool);
    SavedState selectedState() const;
    std::uint64_t selectionVersion() const noexcept { return selectionRevision.load(); }
    void servicePendingChanges();
    ToneController& toneController() noexcept {return tone;}
    juce::AudioProcessorValueTreeState parameters;
    ReverbProcessor reverb;
    IrLibrary library;

  private:
    void timerCallback() override { servicePendingChanges(); }
    void parameterChanged(const juce::String &, float) override
    {
        irDirty.store(true, std::memory_order_release);
        lastIrChange.store(juce::Time::getMillisecondCounter(),std::memory_order_release);
    }
    void submit();
    ParameterRefs refs;
    ToneController tone;
    mutable std::mutex stateMutex;
    mutable SavedState selected;
    std::atomic<std::uint64_t> selectionRevision{1};
    std::atomic<std::uint64_t> submittedRequest{0};
    std::atomic<bool> irDirty{false}, prepared{false};
    std::atomic<juce::uint32> lastIrChange{0};
};
} // namespace ch
