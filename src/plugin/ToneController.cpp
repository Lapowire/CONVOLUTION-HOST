#include "ToneController.h"
#include <algorithm>
namespace ch
{
namespace
{
thread_local const ToneController *localWriter = nullptr;
}
ToneController::ToneController(juce::AudioProcessorValueTreeState &p)
    : parameters(p), definitions(toneParameterDefinitions())
{
    jassert(definitions.size() == externalRevisions.size());
    for (const auto &d : definitions)
    {
        refs.push_back(p.getParameter(d.id));
        raw.push_back(p.getRawParameterValue(d.id));
        parameters.addParameterListener(d.id, this);
    }
}
ToneController::~ToneController()
{
    endGesture();
    for (auto &d : definitions)
        parameters.removeParameterListener(d.id, this);
}
int ToneController::find(const juce::String &id) const
{
    for (std::size_t i = 0; i < definitions.size(); ++i)
        if (definitions[i].id == id)
            return int(i);
    return -1;
}
void ToneController::parameterChanged(const juce::String &id, float)
{
    if (localWriter != this)
        if (int i = find(id); i >= 0)
            externalRevisions[std::size_t(i)].fetch_add(1, std::memory_order_relaxed);
}
void ToneController::write(int index, float value)
{
    auto *p = refs[std::size_t(index)];
    auto *previous = localWriter;
    localWriter = this;
    p->setValueNotifyingHost(p->convertTo0to1(value));
    localWriter = previous;
}
ToneState ToneController::current() const
{
    auto parsed = ToneState::fromParameters(parameters.copyState());
    // Live automation may temporarily invert damping nodes: retain requested values for the UI.
    if (parsed)
        return *parsed;
    ToneState result;
    // Populate directly if structural validation rejects crossing nodes.
    for (std::size_t i = 0; i < 10; ++i)
    {
        auto prefix = "peq" + juce::String(int(i));
        auto value = [&](juce::String id) { return raw[std::size_t(find(id))]->load(); };
        result.eq.gainDb[i] = value("eq" + juce::String(int(i)));
        result.eq.bands[i] = {value(prefix + "Freq"), value(prefix + "Q"),
                              EqType(int(value(prefix + "Type"))), value(prefix + "Enabled") >= 0.5f};
    }
    result.eq.bypass = raw[std::size_t(find("eqBypass"))]->load() >= 0.5f;
    for (std::size_t i = 0; i < 6; ++i)
    {
        auto prefix = "damp" + juce::String(int(i));
        result.damping.nodes[i] = {raw[std::size_t(find(prefix + "Freq"))]->load(),
                                   raw[std::size_t(find(prefix + "Rate"))]->load()};
    }
    result.damping.protectMs = raw[std::size_t(find("dampProtect"))]->load();
    result.damping.bypass = raw[std::size_t(find("dampBypass"))]->load() >= 0.5f;
    return result;
}
void ToneController::beginGesture()
{
    std::lock_guard lock(mutex);
    if (!gesturing)
    {
        pending = {};
        gesturing = true;
    }
}
void ToneController::setParameter(const juce::String &id, float value)
{
    std::lock_guard lock(mutex);
    int index = find(id);
    if (index < 0 || !std::isfinite(value))
        return;
    auto n = std::size_t(index);
    auto revision = externalRevisions[n].load();
    auto *p = refs[n];
    value = p->convertFrom0to1(
        p->convertTo0to1(juce::jlimit(definitions[n].minimum, definitions[n].maximum, value)));
    if (raw[n]->load() == value)
        return;
    bool single = !gesturing;
    if (single)
        beginGesture();
    auto existing = std::find_if(pending.changes.begin(), pending.changes.end(),
                                 [&](auto &c) { return c.index == index; });
    if (existing == pending.changes.end())
    {
        p->beginChangeGesture();
        pending.changes.push_back({index, raw[n]->load(), value, revision});
    }
    else
    {
        if (existing->externalRevision != revision)
        {
            existing->before = raw[n]->load();
            existing->externalRevision = revision;
        }
        existing->after = value;
    }
    write(index, value);
    if (single)
        endGesture();
}
void ToneController::endGesture()
{
    std::lock_guard lock(mutex);
    if (!gesturing)
        return;
    for (auto &c : pending.changes)
        refs[std::size_t(c.index)]->endChangeGesture();
    std::erase_if(pending.changes,
                  [&](auto &c)
                  {
                      return c.before == c.after ||
                             externalRevisions[std::size_t(c.index)].load() != c.externalRevision;
                  });
    if (!pending.changes.empty() || pending.beforeMemories)
    {
        undoStack.push_back(std::move(pending));
        if (undoStack.size() > 128)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    pending = {};
    gesturing = false;
}
bool ToneController::canUndo() const
{
    std::lock_guard lock(mutex);
    return !undoStack.empty();
}
bool ToneController::canRedo() const
{
    std::lock_guard lock(mutex);
    return !redoStack.empty();
}
void ToneController::history(bool redoDirection)
{
    std::lock_guard lock(mutex);
    endGesture();
    auto &source = redoDirection ? redoStack : undoStack;
    auto &destination = redoDirection ? undoStack : redoStack;
    if (source.empty())
        return;
    auto edit = std::move(source.back());
    source.pop_back();
    for (auto &c : edit.changes)
    {
        auto n = std::size_t(c.index);
        float expected = redoDirection ? c.before : c.after;
        if (externalRevisions[n].load() == c.externalRevision && std::abs(raw[n]->load() - expected) < 1e-4f)
        {
            refs[n]->beginChangeGesture();
            write(c.index, redoDirection ? c.after : c.before);
            refs[n]->endChangeGesture();
        }
    }
    auto &metadata = redoDirection ? edit.afterMemories : edit.beforeMemories;
    if (metadata)
        memories = *metadata;
    destination.push_back(std::move(edit));
}
void ToneController::undo()
{
    history(false);
}
void ToneController::redo()
{
    history(true);
}
ToneMemories ToneController::snapshot() const
{
    std::lock_guard lock(mutex);
    memories.slots[std::size_t(memories.active)] = current();
    return memories;
}
void ToneController::restore(const ToneMemories &m)
{
    std::lock_guard lock(mutex);
    endGesture();
    memories = m;
    undoStack.clear();
    redoStack.clear();
}
void ToneController::applyState(const ToneState &state)
{
    for (auto p : state.parametersTree())
        setParameter(p["id"].toString(), float(p["value"]));
}
void ToneController::selectSlot(int slot)
{
    std::lock_guard lock(mutex);
    if (slot < 0 || slot > 1 || slot == memories.active)
        return;
    endGesture();
    auto previous = snapshot();
    auto target = memories.slots[std::size_t(slot)];
    beginGesture();
    pending.beforeMemories = previous;
    memories.active = slot;
    applyState(target);
    pending.afterMemories = snapshot();
    endGesture();
}
void ToneController::copySlot(int from, int to)
{
    std::lock_guard lock(mutex);
    if (from < 0 || from > 1 || to < 0 || to > 1 || from == to)
        return;
    endGesture();
    auto previous = snapshot();
    beginGesture();
    pending.beforeMemories = previous;
    memories.slots[std::size_t(to)] = memories.slots[std::size_t(from)];
    if (to == memories.active)
        applyState(memories.slots[std::size_t(to)]);
    pending.afterMemories = snapshot();
    endGesture();
}
void ToneController::resetEq()
{
    std::lock_guard lock(mutex);
    beginGesture();
    for (auto value : ToneState{}.parametersTree())
    {
        auto id = value["id"].toString();
        if (id.startsWith("eq") || id.startsWith("peq"))
            setParameter(id, float(value["value"]));
    }
    endGesture();
}
void ToneController::resetDamping()
{
    std::lock_guard lock(mutex);
    beginGesture();
    for (auto value : ToneState{}.parametersTree())
    {
        auto id = value["id"].toString();
        if (id.startsWith("damp"))
            setParameter(id, float(value["value"]));
    }
    endGesture();
}
} // namespace ch
