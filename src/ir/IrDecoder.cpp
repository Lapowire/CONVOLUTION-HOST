#include "IrDecoder.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
#include <limits>
namespace ch {
std::size_t IrDecoder::estimateBytes(int samples, int routes, int blockSize) {
 if(samples<1 || routes<1 || blockSize<1) return budgetBytes+1;
 // Conservative bound: FFT spectra, histories, scratch and two live engines.
 return std::size_t(samples)*std::size_t(routes)*64ull + std::size_t(routes)*std::size_t(blockSize)*256ull + 4ull*1024*1024;
}
DecodeResult IrDecoder::decode(const IrRequest& req) const {
 auto fail=[](const char* s) { return DecodeResult{nullptr,s}; };
 if(!std::isfinite(req.sampleRate) || req.sampleRate<8000 || req.sampleRate>384000)
  return fail("Unsupported sample rate");
 for(double v:{req.startMs,req.endMs,req.fadeInMs,req.fadeOutMs}) if(!std::isfinite(v)||v<0) return fail("Invalid IR selection");
 if(!req.file.hasFileExtension("wav")) return fail("Select a WAV impulse response");
 juce::WavAudioFormat fmt;
 auto stream=req.file.createInputStream();
 if(!stream) return fail("Impulse file is missing or unreadable; locate it again");
 std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(stream.release(),true));
 if(!reader || reader->lengthInSamples<1 || reader->numChannels<1 || reader->numChannels>16 || !std::isfinite(reader->sampleRate) || reader->sampleRate<8000 || reader->sampleRate>384000)
  return fail("Invalid or unsupported WAV impulse response");
 if(reader->lengthInSamples/reader->sampleRate>30.000001) return fail("IR exceeds the 30 second limit");
 const auto start=juce::int64(std::llround(req.startMs*reader->sampleRate/1000));
 const auto end=req.endMs>0?juce::int64(std::llround(req.endMs*reader->sampleRate/1000)):reader->lengthInSamples;
 if(start<0 || end>reader->lengthInSamples || end<=start) return fail("IR trim must select a non-empty region inside the file");
 const int sourceN=int(end-start), channels=int(reader->numChannels);
 const int destN=int(std::ceil(sourceN*req.sampleRate/reader->sampleRate));
 if(destN<1 || estimateBytes(destN,channels,1024)>budgetBytes) return fail("IR exceeds the 512 MiB engine budget");
 juce::AudioBuffer<float> source(channels,sourceN+8); source.clear();
 if(!reader->read(&source,0,sourceN,start,true,true)) return fail("Could not decode WAV samples");
 for(int c=0;c<channels;++c) for(int i=0;i<sourceN;++i)
  if(!std::isfinite(source.getSample(c,i))) return fail("IR contains non-finite samples");
 auto ir=std::make_shared<PreparedIr>(); ir->file=req.file; ir->sampleRate=req.sampleRate;
 ir->originalSampleRate=reader->sampleRate; ir->samples.setSize(channels,destN);
 for(int c=0;c<channels;++c) {
  if(req.sampleRate==reader->sampleRate) ir->samples.copyFrom(c,0,source,c,0,destN);
  else {
   juce::LagrangeInterpolator interp;
   interp.process(reader->sampleRate/req.sampleRate,source.getReadPointer(c),ir->samples.getWritePointer(c),destN,sourceN+8,0);
  }
 }
 const int inN=juce::jlimit(0,destN,int(std::min(req.fadeInMs*req.sampleRate/1000,double(destN))));
 const int outN=juce::jlimit(0,destN,int(std::min(req.fadeOutMs*req.sampleRate/1000,double(destN))));
 for(int i=0;i<destN;++i) {
  float g=1;
  if(inN>0 && i<inN) g*=float(i)/float(std::max(1,inN-1));
  if(outN>0 && i>=destN-outN) g*=float(destN-1-i)/float(std::max(1,outN-1));
  for(int c=0;c<channels;++c) ir->samples.setSample(c,i,ir->samples.getSample(c,i)*g);
 }
 float peak=0; for(int c=0;c<channels;++c) peak=std::max(peak,ir->samples.getMagnitude(c,0,destN));
 if(req.normalise && peak>0) ir->samples.applyGain(1.0f/peak);
 auto hashStream=req.file.createInputStream(); if(!hashStream) return fail("IR became unreadable while loading");
 ir->hash=juce::SHA256(*hashStream).toHexString();
 const int bins=std::min(1024,destN); ir->waveform.assign(std::size_t(bins),0); ir->decayDb.assign(std::size_t(bins),-120);
 std::vector<double> energy(std::size_t(bins),0);
 for(int i=0;i<destN;++i) {
  const int bin=std::min(bins-1,int(std::int64_t(i)*bins/destN));
  for(int c=0;c<channels;++c) {
   const auto s=ir->samples.getSample(c,i); if(!std::isfinite(s)) return fail("IR processing produced invalid samples");
   ir->waveform[std::size_t(bin)]=std::max(ir->waveform[std::size_t(bin)],std::abs(s));
   energy[std::size_t(bin)]+=double(s)*s;
  }
 }
 for(int i=bins-2;i>=0;--i) energy[std::size_t(i)]+=energy[std::size_t(i+1)];
 const double total=energy[0];
 for(int i=0;i<bins;++i) ir->decayDb[std::size_t(i)]=total>0?float(10*std::log10(std::max(1.e-12,energy[std::size_t(i)]/total))):-120.f;
 ir->estimatedBytes=estimateBytes(destN,channels,1024);
 return {std::move(ir),{}};
}
}
