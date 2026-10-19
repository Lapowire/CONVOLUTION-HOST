#include "PluginProcessor.h"
namespace ch {
namespace {constexpr std::array<const char*,5> irIds{"trimStart","trimEnd","fadeIn","fadeOut","normalise"};}
PluginProcessor::PluginProcessor(juce::File prefs):AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"PARAMETERS",createParameterLayout()),library(std::move(prefs)),refs(parameters) {
 for(auto id:irIds)parameters.addParameterListener(id,this);startTimerHz(20);
}
PluginProcessor::~PluginProcessor(){stopTimer();for(auto id:irIds)parameters.removeParameterListener(id,this);}
void PluginProcessor::prepareToPlay(double sr,int block) {reverb.prepare(sr,std::max(1,block),getTotalNumInputChannels());setLatencySamples(0);prepared.store(true);submit();}
void PluginProcessor::releaseResources(){prepared.store(false);}
void PluginProcessor::reset(){reverb.reset();}
void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer& midi) {midi.clear();reverb.process(buffer,refs.read());}
bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
 const auto input=layout.getMainInputChannelSet(),output=layout.getMainOutputChannelSet();return input==output && (input==juce::AudioChannelSet::mono() || input==juce::AudioChannelSet::stereo());
}
double PluginProcessor::getTailLengthSeconds() const {return reverb.loader().durationSeconds()+refs.values[4]->load(std::memory_order_relaxed)/1000.;}
juce::AudioProcessorEditor* PluginProcessor::createEditor(){return new juce::GenericAudioProcessorEditor(*this);}
SavedState PluginProcessor::selectedState() const {
 SavedState state;{std::lock_guard lock(stateMutex);state=selected;}
 auto status=reverb.loader().status();if(status.ir && !status.loading && status.error.isEmpty() && status.requestId==submittedRequest.load() && status.ir->file==state.request.file)state.hash=status.ir->hash;
 state.request.startMs=refs.values[10]->load();state.request.endMs=refs.values[11]->load();state.request.fadeInMs=refs.values[12]->load();state.request.fadeOutMs=refs.values[13]->load();state.request.normalise=refs.values[14]->load()>=0.5f;
 return state;
}
void PluginProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=selectedState();state.parameters=parameters.copyState();auto tree=StateStore::encode(state);
 juce::MemoryOutputStream stream(dest,false);tree.writeToStream(stream);
}
void PluginProcessor::setStateInformation(const void* data,int size) {
 if(!data || size<1 || size>4*1024*1024)return;
 SavedState state;if(!StateStore::decode(juce::ValueTree::readFromData(data,std::size_t(size)),state))return;
 submittedRequest.store(0);parameters.replaceState(state.parameters);{std::lock_guard lock(stateMutex);selected=std::move(state);}
 irDirty.store(false);if(prepared.load())submit();
}
void PluginProcessor::loadIr(const juce::File& file,RoutingModel routing) {
 submittedRequest.store(0);
 {std::lock_guard lock(stateMutex);selected.request.file=file;selected.routing=std::move(routing);selected.hash={};selected.relativePath={};
  for(auto root:library.rootPaths()){juce::File dir(root);if(file.isAChildOf(dir)){selected.relativePath=file.getRelativePathFrom(dir);break;}}}
 if(prepared.load())submit();
}
void PluginProcessor::setRouting(RoutingModel model) {{std::lock_guard lock(stateMutex);selected.routing=std::move(model);}if(prepared.load())submit();}
void PluginProcessor::servicePendingChanges(){if(irDirty.exchange(false) && prepared.load())submit();}
void PluginProcessor::submit() {
 auto state=selectedState();if(state.request.file==juce::File{})return;
 state.request.expectedHash=state.hash;state.request.relativePath=state.relativePath;state.request.searchRoots=library.rootPaths();
 submittedRequest.store(reverb.loader().request(std::move(state.request),std::move(state.routing)));
}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new ch::PluginProcessor();}
