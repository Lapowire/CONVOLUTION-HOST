#include "TestSupport.h"
#include "audio/ConvolutionEngine.h"
#include <random>
TEST(convolution_matches_reference_all_routes_and_blocks) {
 for(double sr:{44100.,48000.,96000.}) for(int block:{64,256,1024}) for(int channels:{1,2}) {
  auto ir=std::make_shared<ch::PreparedIr>(); ir->sampleRate=sr;
  const int irChannels=channels==1?1:4; ir->samples.setSize(irChannels,513); ir->samples.clear();
  for(int c=0;c<irChannels;++c) for(int i=0;i<513;++i) ir->samples.setSample(c,i,float((c+1)*0.01*std::exp(-i/80.)*std::cos(i*0.11)));
  auto routing=channels==1?ch::RoutingModel::diagonal(1,1):ch::RoutingModel{{{0,0,0,1},{0,1,1,1},{1,0,2,1},{1,1,3,1}}};
  ch::ConvolutionEngine engine(ir,routing); engine.prepare(sr,block,channels);
  constexpr int length=2200; juce::AudioBuffer<float> source(channels,length),actual(channels,length); source.clear();actual.clear();
  for(int c=0;c<channels;++c) for(int i=0;i<900;++i) source.setSample(c,i,float(std::sin(i*0.23+c)*0.2));
  int offset=0; while(offset<length) {
   const int n=std::min(length-offset,offset%3==0?block:std::max(1,block/3));
   juce::AudioBuffer<float> b(channels,n); for(int c=0;c<channels;++c)b.copyFrom(c,0,source,c,offset,n);
   engine.process(b); for(int c=0;c<channels;++c)actual.copyFrom(c,offset,b,c,0,n); offset+=n;
  }
  for(int c=0;c<channels;++c) for(int i=0;i<length;++i) {
   float expected=0; for(auto r:routing.routes) if(r.output==c)
    for(int k=0;k<=std::min(i,512);++k) expected+=source.getSample(r.input,i-k)*ir->samples.getSample(r.irChannel,k)*r.gain;
   test::near(actual.getSample(c,i),expected);
  }
  REQUIRE(engine.latencySamples()==0); engine.reset(); juce::AudioBuffer<float> silence(channels,block);silence.clear();engine.process(silence);test::near(silence.getMagnitude(0,block),0);
 }
}
TEST(diagonal_has_no_crossfeed) {
 auto ir=std::make_shared<ch::PreparedIr>(); ir->samples.setSize(2,1); ir->samples.setSample(0,0,0.25f); ir->samples.setSample(1,0,0.5f);
 ch::ConvolutionEngine engine(ir,ch::RoutingModel::diagonal(2,2));engine.prepare(48000,64,2);
 juce::AudioBuffer<float> b(2,64);b.clear();b.setSample(0,0,1);engine.process(b);
 test::near(b.getSample(0,0),0.25f);test::near(b.getMagnitude(1,0,64),0);
}
#include "RealtimeGuard.h"
TEST(convolution_does_not_allocate_or_destroy_on_audio_thread) {
 auto ir=std::make_shared<ch::PreparedIr>();ir->samples.setSize(2,8192);ir->samples.clear();ir->samples.setSample(0,0,1);ir->samples.setSample(1,0,1);
 ch::ConvolutionEngine engine(ir,ch::RoutingModel::diagonal(2,2));engine.prepare(48000,256,2);juce::AudioBuffer<float> b(2,256);b.clear();
 {test::RealtimeGuard guard;for(int n=0;n<100;++n)engine.process(b);}
 REQUIRE(test::realtimeAllocations==0);
}
