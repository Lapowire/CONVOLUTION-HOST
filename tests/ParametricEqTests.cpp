#include "TestSupport.h"
#include "RealtimeGuard.h"
#include "audio/ParametricEq.h"
TEST(parametric_eq_analytic_and_realtime_response)
{
    for (double sr : {44100.,48000.,96000.})
    {
        ch::EqSettings s;
        s.bands[5].frequencyHz=1000;
        s.gainDb[5]=12;
        test::near(float(ch::ParametricEq::magnitude(s,sr,1000)),std::pow(10.f,12.f/20),1e-3f);
        ch::ParametricEq eq; eq.prepare(sr,2); eq.setTargets(s);
        juce::AudioBuffer<float> b(2,256);
        double input=0, output=0; int pos=0;
        for(int block=0;block<100;++block)
        {
            b.clear();
            for(int i=0;i<256;++i) b.setSample(0,i,float(std::sin(2*juce::MathConstants<double>::pi*1000*pos++/sr)));
            if(block>80) for(int i=0;i<256;++i) input+=double(b.getSample(0,i))*b.getSample(0,i);
            {test::RealtimeGuard guard; eq.process(b);}
            REQUIRE(test::realtimeAllocations==0);
            for(int i=0;i<256;++i){ REQUIRE(b.getSample(1,i)==0); if(block>80)output+=double(b.getSample(0,i))*b.getSample(0,i); }
        }
        test::near(float(10*std::log10(output/input)),12,0.15f);
        s.bands[5].type=ch::EqType::notch;
        REQUIRE(ch::ParametricEq::magnitude(s,sr,1000)<1e-5);
        s.bypass=true;
        test::near(float(ch::ParametricEq::magnitude(s,sr,1000)),1);
    }
}
TEST(parametric_eq_extreme_automation_is_finite_and_allocation_free)
{
    for(double sr:{8000.,44100.,96000.})
    {
        ch::ParametricEq eq; eq.prepare(sr,16);
        ch::EqSettings s; juce::AudioBuffer<float> b(16,37);
        for(int k=0;k<300;++k)
        {
            for(int i=0;i<10;++i){s.bands[i].frequencyHz=k%2?20:20000;s.bands[i].q=k%2?0.1f:18;s.bands[i].type=ch::EqType(k%4);s.gainDb[i]=k%2?12:-12;s.bands[i].enabled=k%5!=0;}
            s.bypass=k%7==0; b.clear(); b.setSample(0,0,0.01f);
            { test::RealtimeGuard guard; eq.setTargets(s); eq.process(b); }
            REQUIRE(test::realtimeAllocations==0);
            for(int c=0;c<16;++c)for(int i=0;i<37;++i) REQUIRE(std::isfinite(b.getSample(c,i)));
        }
    }
}
