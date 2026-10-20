#include "SpectralDamping.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
namespace ch
{
namespace
{
int orderFor(double sr) noexcept
{ return juce::jlimit(9,13,int(std::llround(std::log2(2048*sr/48000)))); }
}
bool DampingSettings::isValid() const noexcept
{
    if(!std::isfinite(protectMs)||protectMs<0||protectMs>200)return false;
    float previous=0;
    for(auto n:nodes)
    {
        if(!std::isfinite(n.frequencyHz)||n.frequencyHz<20||n.frequencyHz>20000||n.frequencyHz<=previous||!std::isfinite(n.attenuationDbPerSecond)||n.attenuationDbPerSecond<0||n.attenuationDbPerSecond>60)return false;
        previous=n.frequencyHz;
    }
    return true;
}
bool DampingSettings::isNeutral() const noexcept
{
    if(bypass)return true;
    for(auto n:nodes)if(n.attenuationDbPerSecond!=0)return false;
    return true;
}
float DampingSettings::attenuationAt(double f) const noexcept
{
    if(!std::isfinite(f)||f<=nodes.front().frequencyHz)return nodes.front().attenuationDbPerSecond;
    for(std::size_t i=1;i<nodes.size();++i)if(f<nodes[i].frequencyHz)
    {
        double t=std::log(f/nodes[i-1].frequencyHz)/std::log(nodes[i].frequencyHz/nodes[i-1].frequencyHz);
        return float(nodes[i-1].attenuationDbPerSecond+t*(nodes[i].attenuationDbPerSecond-nodes[i-1].attenuationDbPerSecond));
    }
    return nodes.back().attenuationDbPerSecond;
}
std::size_t SpectralDamping::scratchBytes(double sr) noexcept
{
    if(!std::isfinite(sr)||sr<8000||sr>384000)return 16*1024*1024+1;
    return std::size_t(1<<orderFor(sr))*64+65536;
}
juce::Result SpectralDamping::apply(juce::AudioBuffer<float>& buffer,double sr,const DampingSettings& settings,const std::function<bool()>& cancelled)
{
    if(!settings.isValid()||scratchBytes(sr)>16*1024*1024)return juce::Result::fail("Invalid damping settings or scratch budget");
    if(cancelled&&cancelled())return juce::Result::fail("IR preparation superseded");
    if(settings.isNeutral())return juce::Result::ok();
    const int order=orderFor(sr),n=1<<order,hop=n/4,length=buffer.getNumSamples();
    juce::dsp::FFT fft(order);
    std::vector<float> data(std::size_t(2*n),0),window(std::size_t(n),0),attenuation(std::size_t(n/2+1),0);
    std::vector<double> sum(std::size_t(n),0),weight(std::size_t(n),0);
    for(int i=0;i<n;++i)window[std::size_t(i)]=float(std::sqrt(0.5-0.5*std::cos(2*juce::MathConstants<double>::pi*i/n)));
    for(int k=0;k<=n/2;++k)attenuation[std::size_t(k)]=settings.attenuationAt(k*sr/n);
    for(int channel=0;channel<buffer.getNumChannels();++channel)
    {
        std::fill(sum.begin(),sum.end(),0);std::fill(weight.begin(),weight.end(),0);
        for(int start=-n+hop;start<length;start+=hop)
        {
            if(cancelled&&cancelled())return juce::Result::fail("IR preparation superseded");
            std::fill(data.begin(),data.end(),0);
            for(int i=0;i<n;++i){int position=start+i;if(position>=0&&position<length)data[std::size_t(i)]=buffer.getSample(channel,position)*window[std::size_t(i)];}
            fft.performRealOnlyForwardTransform(data.data(),true);
            double seconds=std::max(0.,(start+n*0.5)/sr-settings.protectMs/1000.);
            for(int k=0;k<=n/2;++k)
            {
                float gain=float(std::pow(10.,-attenuation[std::size_t(k)]*seconds/20));
                data[std::size_t(2*k)]*=gain;data[std::size_t(2*k+1)]*=gain;
            }
            fft.performRealOnlyInverseTransform(data.data());
            for(int i=0;i<n;++i)
            {
                int position=start+i;if(position<0||position>=length)continue;
                auto slot=std::size_t(position%n);double w=window[std::size_t(i)];
                sum[slot]+=data[std::size_t(i)]*w;weight[slot]+=w*w;
            }
            // Future windows never read the finalized prefix: bounded in-place overlap/add.
            for(int position=std::max(0,start);position<std::min(length,start+hop);++position)
            {
                auto slot=std::size_t(position%n);float sample=weight[slot]>0?float(sum[slot]/weight[slot]):0;
                if(!std::isfinite(sample))return juce::Result::fail("Damping produced invalid samples");
                buffer.setSample(channel,position,sample);sum[slot]=weight[slot]=0;
            }
        }
    }
    return juce::Result::ok();
}
}
