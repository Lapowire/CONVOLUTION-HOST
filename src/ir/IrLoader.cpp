#include "IrLoader.h"
#include "state/StateStore.h"
#include <chrono>
namespace ch {
IrLoader::IrLoader():worker([this]{run();}){}
IrLoader::~IrLoader() {
 {std::lock_guard lock(mutex);stopping=true;}wake.notify_one();worker.join();
 destroy(ready.exchange(nullptr));destroy(active);destroy(fading);collect();
}
void IrLoader::destroy(Packet* p) {if(p){delete p;live.fetch_sub(1,std::memory_order_release);}}
bool IrLoader::retire(Packet* p) noexcept {
 if(!p)return true;
 const auto w=writeIndex.load(std::memory_order_relaxed); const auto next=(w+1)%retired.size();
 if(next==readIndex.load(std::memory_order_acquire))return false;
 retired[w]=p;writeIndex.store(unsigned(next),std::memory_order_release);return true;
}
void IrLoader::collect() {
 auto r=readIndex.load(std::memory_order_relaxed);
 while(r!=writeIndex.load(std::memory_order_acquire)) {
  destroy(retired[r]);r=unsigned((r+1)%retired.size());readIndex.store(r,std::memory_order_release);
 }
}
void IrLoader::configure(double sr,int block,int channels) {
 { std::lock_guard lock(mutex); rate=sr;blockSize=juce::jlimit(1,65536,block);channelCount=channels;
   generation.fetch_add(1);latest.fetch_add(1);pending.reset();current.loading=false; }
 destroy(ready.exchange(nullptr));destroy(active);active=nullptr;destroy(fading);fading=nullptr;
 activeFlag.store(false);duration.store(0);accepted.store(0);fadePosition=0;fadeLength=std::max(1,int(sr*0.05));
 oldBuffer.setSize(channels,blockSize);oldBuffer.clear();
}
std::uint64_t IrLoader::request(IrRequest req,RoutingModel routing) {
 std::uint64_t id;
 {std::lock_guard lock(mutex);id=latest.fetch_add(1)+1;req.sampleRate=rate;
  pending=Job{std::move(req),std::move(routing),rate,blockSize,channelCount,id,generation.load()};
  current.loading=true;current.error={};current.requestId=id;}
 wake.notify_one();return id;
}
IrLoader::Status IrLoader::status() const {std::lock_guard lock(mutex);return current;}
void IrLoader::run() {
 for(;;) {
  collect();
  // Only the worker publishes; inspect the separate id, never an unowned pointer.
  if(readyId.load()!=latest.load())destroy(ready.exchange(nullptr,std::memory_order_acq_rel));
  std::optional<Job> job;
  {std::unique_lock lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(3),[this]{return stopping || (pending && live.load()<2);});
   if(stopping)break;
   if(pending && live.load()<2){job=std::move(pending);pending.reset();}}
  if(!job)continue;
  DecodeResult decoded;
  if(job->request.expectedHash.isNotEmpty()) {
   SavedState saved;saved.request=job->request;saved.hash=job->request.expectedHash;saved.relativePath=job->request.relativePath;
   const auto resolved=StateStore::resolve(saved,job->request.searchRoots);
   if(resolved==juce::File{})decoded.error="Saved IR is missing or its content changed; locate the original WAV";
   else {auto request=job->request;request.file=resolved;decoded=IrDecoder{}.decode(request);
    if(decoded && decoded.ir->hash!=job->request.expectedHash){decoded.ir.reset();decoded.error="IR content changed while loading";}}
  } else decoded=IrDecoder{}.decode(job->request);
  std::unique_ptr<ConvolutionEngine> engine;
  if(decoded && job->id==latest.load()) {
   auto mapping=job->routing.routes.empty()?RoutingModel::diagonal(job->channels,decoded.ir->samples.getNumChannels()):job->routing;
   try {engine=std::make_unique<ConvolutionEngine>(decoded.ir,std::move(mapping));engine->prepare(job->rate,job->block,job->channels);}
   catch(const std::exception& e){decoded.error=juce::String::fromUTF8(e.what());engine.reset();}
  }
  if(job->id!=latest.load() || job->generation!=generation.load())continue;
  if(engine) {
   auto* p=new Packet{std::move(engine),job->id,job->generation};live.fetch_add(1,std::memory_order_release);
   readyId.store(job->id);destroy(ready.exchange(p,std::memory_order_acq_rel));
  }
  {std::lock_guard lock(mutex);if(job->id==latest.load()) {
   current.loading=false;current.error=decoded.error;
   if(decoded.error.isEmpty() && decoded.ir)current.ir=std::move(decoded.ir);
  }}
 }
}
void IrLoader::processWet(juce::AudioBuffer<float>& buffer) noexcept {
 if(fading && fadePosition>=fadeLength && retire(fading))fading=nullptr;
 if(!fading) if(auto* p=ready.exchange(nullptr,std::memory_order_acq_rel)) {
  if(p->generation!=generation.load(std::memory_order_acquire) || p->id!=latest.load(std::memory_order_acquire)) {
   const bool ok=retire(p);jassert(ok);juce::ignoreUnused(ok);
  } else {
   fading=active;active=p;fadePosition=0;
   activeFlag.store(true,std::memory_order_release);duration.store(active->engine->durationSeconds());accepted.store(p->id,std::memory_order_release);
  }
 }
 if(!active){buffer.clear();return;}
 for(int offset=0;offset<buffer.getNumSamples();offset+=blockSize) {
  const int n=std::min(blockSize,buffer.getNumSamples()-offset);
  juce::AudioBuffer<float> view(buffer.getArrayOfWritePointers(),buffer.getNumChannels(),offset,n);
  if(fading)for(int c=0;c<view.getNumChannels();++c)oldBuffer.copyFrom(c,0,view,c,0,n);
  active->engine->process(view);
  if(fading) {
   juce::AudioBuffer<float> old(oldBuffer.getArrayOfWritePointers(),view.getNumChannels(),n);fading->engine->process(old);
   for(int i=0;i<n;++i){const float alpha=std::min(1.f,float(fadePosition++)/float(fadeLength));
    for(int c=0;c<view.getNumChannels();++c)view.setSample(c,i,old.getSample(c,i)*(1-alpha)+view.getSample(c,i)*alpha);}
   if(fadePosition>=fadeLength && retire(fading))fading=nullptr;
  }
 }
}
void IrLoader::reset() noexcept {if(active)active->engine->reset();if(fading)fading->engine->reset();}
}
