#pragma once
#include "state/ToneState.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <mutex>
namespace ch
{
class ToneController : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ToneController(juce::AudioProcessorValueTreeState&);
    ~ToneController() override;
    void beginGesture();
    void setParameter(const juce::String&,float plainValue);
    void endGesture();
    void undo();void redo();
    bool canUndo() const;bool canRedo() const;
    void selectSlot(int);void copySlot(int,int);
    void resetEq();void resetDamping();
    ToneMemories snapshot() const;
    void restore(const ToneMemories&);
    ToneState current() const;
private:
    struct Change {int index;float before,after;std::uint64_t externalRevision;};
    struct Edit {std::vector<Change> changes;std::optional<ToneMemories> beforeMemories,afterMemories;};
    void parameterChanged(const juce::String&,float) override;
    void write(int,float);
    void applyState(const ToneState&);
    void history(bool redo);
    int find(const juce::String&) const;
    juce::AudioProcessorValueTreeState& parameters;
    std::vector<ToneParameter> definitions;
    std::vector<juce::RangedAudioParameter*> refs;
    std::vector<std::atomic<float>*> raw;
    std::array<std::atomic<std::uint64_t>,65> externalRevisions{};
    mutable std::recursive_mutex mutex;
    mutable ToneMemories memories;
    std::vector<Edit> undoStack,redoStack;
    Edit pending;
    bool gesturing=false;
};
}
