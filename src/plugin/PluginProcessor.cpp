#include "PluginProcessor.h"
#include "BuildIdentity.h"
#include "ui/PluginEditor.h"
namespace ch
{
namespace
{
constexpr std::array<const char *, 5> irIds{"trimStart", "trimEnd", "fadeIn", "fadeOut", "normalise"};
}
PluginProcessor::PluginProcessor(juce::File prefs)
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()), library(std::move(prefs)),
      refs(parameters),tone(parameters)
{
    for (auto id : irIds)
        parameters.addParameterListener(id, this);
    for(const auto& d:toneParameterDefinitions(true))if(d.id.startsWith("damp"))parameters.addParameterListener(d.id,this);
    startTimerHz(20);
}
PluginProcessor::~PluginProcessor()
{
    stopTimer();
    for(const auto& d:toneParameterDefinitions(true))if(d.id.startsWith("damp"))parameters.removeParameterListener(d.id,this);
    for (auto id : irIds)
        parameters.removeParameterListener(id, this);
}
void PluginProcessor::prepareToPlay(double sr, int block)
{
    reverb.prepare(sr, std::max(1, block), getTotalNumInputChannels());
    if (auto layout = busLayout())
        reverb.configureWidthPairs(layout->stereoPairs);
    setLatencySamples(0);
    prepared.store(true);
    submit();
}
void PluginProcessor::releaseResources()
{
    prepared.store(false);
}
void PluginProcessor::reset()
{
    reverb.reset();
}
void PluginProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi)
{
    midi.clear();
    reverb.process(buffer, refs.read());
}
bool PluginProcessor::isBusesLayoutSupported(const BusesLayout &layout) const
{
    if (layout.inputBuses.size() != 1 || layout.outputBuses.size() != 1)
        return false;
    const auto input = layout.getMainInputChannelSet(), output = layout.getMainOutputChannelSet();
    return layout.inputBuses.size() == 1 && layout.outputBuses.size() == 1 && input == output &&
           BusLayout::fromHost(input, BusInterpretation::followHost).has_value();
}
double PluginProcessor::getTailLengthSeconds() const
{
    return reverb.loader().durationSeconds() + refs.values[4]->load(std::memory_order_relaxed) / 1000.;
}
juce::AudioProcessorEditor *PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}
SavedState PluginProcessor::selectedState() const
{
    auto status = reverb.loader().status();
    SavedState state;
    {
        std::lock_guard lock(stateMutex);
        if (status.ir && !status.loading && status.error.isEmpty() &&
            status.requestId == submittedRequest.load() && status.ir->file == selected.request.file)
        {
            // Keep verified identity durable while a later edit is pending or fails.
            selected.hash = status.ir->hash;
            selected.request.sourceHashes = status.ir->sourceHashes;
        }
        state = selected;
    }
    state.request.startMs = refs.values[10]->load();
    state.request.endMs = refs.values[11]->load();
    state.request.fadeInMs = refs.values[12]->load();
    state.request.fadeOutMs = refs.values[13]->load();
    state.request.normalise = refs.values[14]->load() >= 0.5f;
    state.request.damping=refs.readDamping();
    return state;
}
void PluginProcessor::getStateInformation(juce::MemoryBlock &dest)
{
    auto state = selectedState();
    state.parameters = parameters.copyState();
    state.toneMemories=tone.snapshot();
    auto tree = StateStore::encode(state);
    tree.setProperty("buildIdentity", CH_BUILD_ID, nullptr);
    juce::MemoryOutputStream stream(dest, false);
    tree.writeToStream(stream);
}
void PluginProcessor::setStateInformation(const void *data, int size)
{
    if (!data || size < 1 || size > 4 * 1024 * 1024)
        return;
    SavedState state;
    if (!StateStore::decode(juce::ValueTree::readFromData(data, std::size_t(size)), state))
        return;
    submittedRequest.store(0);
    auto normalized=parameters.copyState();
    for(auto child:normalized)
    {
        auto id=child["id"].toString();auto* parameter=parameters.getParameter(id);
        if(!parameter)continue;
        float value=parameter->convertFrom0to1(parameter->getDefaultValue());
        for(auto source:state.parameters)if(source["id"].toString()==id)value=float(source["value"]);
        if(!std::isfinite(value))return;
        child.setProperty("value",value,nullptr);
    }
    parameters.replaceState(normalized);
    tone.restore(state.toneMemories);
    {
        std::lock_guard lock(stateMutex);
        selected = std::move(state);
    }
    selectionRevision.fetch_add(1);
    irDirty.store(false);
    if (prepared.load())
        submit();
}
void PluginProcessor::loadIr(const juce::File &file, RoutingModel routing)
{
    submittedRequest.store(0);
    {
        std::lock_guard lock(stateMutex);
        selected.request.file = file;
        selected.request.additionalFiles.clear();
        selected.request.sourceHashes.clear();
        selected.request.additionalRelativePaths.clear();
        if (!routing.automatic && routing.sourceLayout.isEmpty())
            if (auto layout = BusLayout::fromHost(getChannelLayoutOfBus(true, 0), selected.interpretation))
                routing.sourceLayout = layout->identity();
        routing.lfeWetEnabled = selected.routing.lfeWetEnabled;
        selected.routing = std::move(routing);
        selected.hash = {};
        selected.relativePath = {};
        for (auto root : library.rootPaths())
        {
            juce::File dir(root);
            if (file.isAChildOf(dir))
            {
                selected.relativePath = file.getRelativePathFrom(dir);
                break;
            }
        }
    }
    selectionRevision.fetch_add(1);
    if (prepared.load())
        submit();
}
void PluginProcessor::loadTrueStereo(const std::array<juce::File, 4> &files, RoutingModel model)
{
    submittedRequest.store(0);
    auto roots = library.rootPaths();
    {
        std::lock_guard lock(stateMutex);
        selected = SavedState{};
        selected.request.file = files[0];
        for (int i = 1; i < 4; ++i)
            selected.request.additionalFiles.push_back(files[std::size_t(i)]);
        if (model.automatic)
            model = {{{0, 0, 0, 1}, {0, 1, 1, 1}, {1, 0, 2, 1}, {1, 1, 3, 1}}};
        if (!model.automatic && model.sourceLayout.isEmpty())
            if (auto layout = BusLayout::fromHost(getChannelLayoutOfBus(true, 0), selected.interpretation))
                model.sourceLayout = layout->identity();
        model.lfeWetEnabled = selected.routing.lfeWetEnabled;
        selected.routing = std::move(model);
        for (int i = 0; i < 4; ++i)
        {
            auto relative = files[std::size_t(i)].getFileName();
            for (auto root : roots)
            {
                juce::File dir(root);
                if (files[std::size_t(i)].isAChildOf(dir))
                {
                    relative = files[std::size_t(i)].getRelativePathFrom(dir);
                    break;
                }
            }
            if (i == 0)
                selected.relativePath = relative;
            else
                selected.request.additionalRelativePaths.add(relative);
        }
    }
    selectionRevision.fetch_add(1);
    if (prepared.load())
        submit();
}
void PluginProcessor::setRouting(RoutingModel model)
{
    {
        std::lock_guard lock(stateMutex);
        if (!model.automatic && model.sourceLayout.isEmpty())
            if (auto layout = BusLayout::fromHost(getChannelLayoutOfBus(true, 0), selected.interpretation))
                model.sourceLayout = layout->identity();
        model.lfeWetEnabled = selected.routing.lfeWetEnabled;
        selected.routing = std::move(model);
    }
    selectionRevision.fetch_add(1);
    if (prepared.load())
        submit();
}
std::optional<BusLayout> PluginProcessor::busLayout() const
{
    std::lock_guard lock(stateMutex);
    return BusLayout::fromHost(getChannelLayoutOfBus(true, 0), selected.interpretation);
}
void PluginProcessor::setBusInterpretation(BusInterpretation mode)
{
    if (!BusLayout::fromHost(getChannelLayoutOfBus(true, 0), mode))
        return;
    {
        std::lock_guard lock(stateMutex);
        selected.interpretation = mode;
    }
    selectionRevision.fetch_add(1);
    if (prepared.load())
        submit();
}
void PluginProcessor::setLfeWetEnabled(bool enabled)
{
    {
        std::lock_guard lock(stateMutex);
        selected.routing.lfeWetEnabled = enabled;
    }
    selectionRevision.fetch_add(1);
    if (prepared.load())
        submit();
}
void PluginProcessor::servicePendingChanges()
{
    if (prepared.load() && juce::Time::getMillisecondCounter()-lastIrChange.load()>=150 && irDirty.exchange(false))
        submit();
}
void PluginProcessor::submit()
{
    auto state = selectedState();
    if (state.request.file == juce::File{})
    {
        submittedRequest.store(reverb.loader().clear());
        return;
    }
    state.request.expectedHash = state.hash;
    state.request.relativePath = state.relativePath;
    state.request.searchRoots = library.rootPaths();
    submittedRequest.store(reverb.loader().request(
        std::move(state.request), std::move(state.routing),
        BusLayout::fromHost(getChannelLayoutOfBus(true, 0), state.interpretation), true));
}
} // namespace ch
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new ch::PluginProcessor();
}
