#include "TestSupport.h"
#include "WavFixtures.h"
#include "ir/IrDecoder.h"
TEST(decode_pcm_and_float) {
 test::TempDir tmp; juce::AudioBuffer<float> b(2,64); b.clear(); b.setSample(0,0,0.25f); b.setSample(1,0,0.5f);
 for(bool fp:{false,true}) {
  auto file=tmp.wav(fp?"float.wav":"pcm.wav",b,48000,fp);
  auto r=ch::IrDecoder{}.decode({file}); REQUIRE(r); REQUIRE(r.ir->samples.getNumChannels()==2);
  test::near(r.ir->samples.getSample(0,0),0.25f); test::near(r.ir->samples.getSample(1,0),0.5f);
  REQUIRE(r.ir->hash.isNotEmpty()); REQUIRE(!r.ir->waveform.empty());
 }
}
TEST(resample_ir) {
 test::TempDir tmp; juce::AudioBuffer<float> b(1,441); b.clear(); b.setSample(0,0,1);
 ch::IrRequest req{tmp.wav("rate.wav",b,44100)}; auto r=ch::IrDecoder{}.decode(req);
 REQUIRE(r); REQUIRE(r.ir->samples.getNumSamples()==480); REQUIRE(r.ir->sampleRate==48000);
}
TEST(trim_fade_and_common_normalisation) {
 test::TempDir tmp; juce::AudioBuffer<float> b(2,480); for(int i=0;i<480;++i){ b.setSample(0,i,0.25f); b.setSample(1,i,0.5f); }
 ch::IrRequest req{tmp.wav("trim.wav",b)}; req.startMs=1; req.endMs=9; req.fadeInMs=1; req.fadeOutMs=1; req.normalise=true;
 auto r=ch::IrDecoder{}.decode(req); REQUIRE(r); REQUIRE(r.ir->samples.getNumSamples()==384);
 test::near(r.ir->samples.getSample(0,100),0.5f); test::near(r.ir->samples.getSample(1,100),1);
 test::near(r.ir->samples.getSample(0,0),0); test::near(r.ir->samples.getSample(1,383),0);
}
TEST(reject_invalid_ir_and_budget) {
 test::TempDir tmp; REQUIRE(!ch::IrDecoder{}.decode({tmp.dir.getChildFile("missing.wav")}));
 auto f=tmp.dir.getChildFile("broken.wav"); f.replaceWithText("not a wave"); REQUIRE(!ch::IrDecoder{}.decode({f}));
 juce::AudioBuffer<float> b(1,32); b.clear(); b.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
 REQUIRE(!ch::IrDecoder{}.decode({tmp.wav("nan.wav",b)}));
 ch::IrRequest invalid{f}; invalid.sampleRate=0; REQUIRE(!ch::IrDecoder{}.decode(invalid));
 REQUIRE(ch::IrDecoder::estimateBytes(2880000,16,256)>ch::IrDecoder::budgetBytes);
}
TEST(decay_silence_and_energy_are_finite_monotonic) {
 test::TempDir tmp; juce::AudioBuffer<float> b(1,1024); b.clear();
 for(bool silent:{true,false}) {
  if(!silent) for(int i=0;i<1024;++i) b.setSample(0,i,std::exp(-0.01f*i));
  auto r=ch::IrDecoder{}.decode({tmp.wav(silent?"silence.wav":"decay.wav",b)}); REQUIRE(r);
  REQUIRE(!r.ir->decayDb.empty()); float last=0;
  for(float d:r.ir->decayDb){ REQUIRE(std::isfinite(d)); REQUIRE(d<=last+0.0001f); last=d; }
 }
}
TEST(reject_long_ir_and_invalid_trim) {
 test::TempDir tmp; juce::AudioBuffer<float> b(1,48000*31); b.clear();
 auto f=tmp.wav("long.wav",b); REQUIRE(!ch::IrDecoder{}.decode({f}));
 b.setSize(1,480); b.clear(); ch::IrRequest r{tmp.wav("short.wav",b)}; r.startMs=9; r.endMs=1;
 REQUIRE(!ch::IrDecoder{}.decode(r));
}
