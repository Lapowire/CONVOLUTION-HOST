#include "ConvolutionEngine.h"
#include "ir/IrDecoder.h"
#include <stdexcept>
namespace ch {
ConvolutionEngine::ConvolutionEngine(std::shared_ptr<const PreparedIr> p, RoutingModel m):impulse(std::move(p)),model(std::move(m)){}
void ConvolutionEngine::prepare(double sampleRate, int maxBlockSize, int channels) {
 if(!impulse || sampleRate!=impulse->sampleRate || maxBlockSize<1 || maxBlockSize>65536 || !model.validate(channels,channels,impulse->samples.getNumChannels()))
  throw std::runtime_error("Invalid convolution configuration");
 if(IrDecoder::estimateBytes(impulse->samples.getNumSamples(),int(model.routes.size()),maxBlockSize)>IrDecoder::budgetBytes)
  throw std::runtime_error("Routing exceeds the engine memory budget");
 blockSize=maxBlockSize;channelCount=channels;
 input.setSize(channels,blockSize);scratch.setSize(1,blockSize);paths.clear();
 for(auto r:model.routes) {
  auto conv=std::make_unique<juce::dsp::Convolution>(juce::dsp::Convolution::NonUniform{256},queue);
  juce::AudioBuffer<float> mono(1,impulse->samples.getNumSamples());
  mono.copyFrom(0,0,impulse->samples,r.irChannel,0,mono.getNumSamples());
  conv->loadImpulseResponse(std::move(mono),sampleRate,juce::dsp::Convolution::Stereo::no,juce::dsp::Convolution::Trim::no,juce::dsp::Convolution::Normalise::no);
  conv->prepare({sampleRate,static_cast<juce::uint32>(blockSize),1});
  paths.push_back({r,std::move(conv)});
 }
}
void ConvolutionEngine::process(juce::AudioBuffer<float>& b) noexcept {
 if(blockSize<1 || b.getNumChannels()!=channelCount) { b.clear();return; }
 for(int offset=0;offset<b.getNumSamples();offset+=blockSize) {
  const int n=std::min(blockSize,b.getNumSamples()-offset);
  for(int c=0;c<channelCount;++c) {input.copyFrom(c,0,b,c,offset,n);b.clear(c,offset,n);}
  for(auto& p:paths) {
   scratch.copyFrom(0,0,input,p.route.input,0,n);
   auto block=juce::dsp::AudioBlock<float>(scratch).getSubBlock(0,std::size_t(n));
   p.convolution->process(juce::dsp::ProcessContextReplacing<float>(block));
   b.addFrom(p.route.output,offset,scratch,0,0,n,p.route.gain);
  }
 }
}
void ConvolutionEngine::reset() noexcept {for(auto& p:paths)p.convolution->reset();}
}
