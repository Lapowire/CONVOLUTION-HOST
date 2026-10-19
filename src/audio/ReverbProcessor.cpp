#include "ReverbProcessor.h"
#include <cmath>
namespace ch
{
namespace
{
float bounded(float v, float lo, float hi, float fallback)
{
    return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
}
std::array<double, 5> peak(double rate, double freq, double db)
{
    if (freq >= rate * 0.5 || std::abs(db) < 1.e-6)
        return {1, 0, 0, 0, 0};
    const double a = std::pow(10, db / 40), w = 2 * juce::MathConstants<double>::pi * freq / rate,
                 alpha = std::sin(w) / (2 * 1.4), c = std::cos(w), norm = 1 / (1 + alpha / a);
    return {(1 + alpha * a) * norm, -2 * c * norm, (1 - alpha * a) * norm, -2 * c * norm,
            (1 - alpha / a) * norm};
}
std::array<double, 5> pass(double rate, double freq, bool high)
{
    const double w = 2 * juce::MathConstants<double>::pi * std::min(freq, rate * 0.45) / rate,
                 c = std::cos(w), alpha = std::sin(w) / (2 * 0.7071067811865476), norm = 1 / (1 + alpha);
    const double b0 = (high ? 1 + c : 1 - c) * 0.5;
    return {b0 * norm, (high ? -2 : 2) * b0 * norm, b0 * norm, -2 * c * norm, (1 - alpha) * norm};
}
} // namespace
void ReverbProcessor::Filter::step() noexcept
{
    for (std::size_t i = 0; i < 5; ++i)
        current[i] += (target[i] - current[i]) * smoothing;
}
float ReverbProcessor::Filter::tick(float x, int ch) noexcept
{
    const auto c = std::size_t(ch);
    const double y = current[0] * x + z1[c];
    z1[c] = current[1] * x - current[3] * y + z2[c];
    z2[c] = current[2] * x - current[4] * y;
    return float(y);
}
void ReverbProcessor::prepare(double sr, int maxBlockSize, int channels)
{
    rate = sr;
    blockSize = juce::jlimit(1, 65536, maxBlockSize);
    channelCount = channels;
    wet.setSize(channels, blockSize);
    dry.setSize(channels, blockSize);
    original.setSize(channels, blockSize);
    delay.setSize(channels, int(sr * 0.5) + 2);
    delay.clear();
    writePosition = 0;
    delayOld = delayNew = 0;
    delayBlend = 1;
    initial = true;
    for (auto &s : smooth)
        s.reset(sr, 0.02);
    for (auto &f : filters)
    {
        f = Filter{};
        f.smoothing = 1 - std::exp(-1 / (sr * 0.02));
    }
    impulses.configure(sr, blockSize, channels);
    inputPeak.store(0);
    outputPeak.store(0);
}
void ReverbProcessor::reset() noexcept
{
    impulses.reset();
    delay.clear();
    writePosition = 0;
    for (auto &f : filters)
    {
        f.z1.fill(0);
        f.z2.fill(0);
    }
}
void ReverbProcessor::process(juce::AudioBuffer<float> &buffer, const ParameterSnapshot &p) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() != channelCount || blockSize < 1)
        return;
    const std::array<float, 7> targets{juce::Decibels::decibelsToGain(bounded(p.inputGain, -24, 24, 0)),
                                       juce::Decibels::decibelsToGain(bounded(p.outputGain, -24, 24, 0)),
                                       juce::Decibels::decibelsToGain(bounded(p.wetGain, -60, 12, 0)),
                                       bounded(p.mix, 0, 100, 25) / 100,
                                       bounded(p.width, 0, 200, 100) / 100,
                                       p.bypass ? 1.f : 0.f,
                                       0.f};
    for (std::size_t i = 0; i < smooth.size(); ++i)
    {
        if (initial)
            smooth[i].setCurrentAndTargetValue(targets[i]);
        else
            smooth[i].setTargetValue(targets[i]);
    }
    for (std::size_t i = 0; i < 10; ++i)
        filters[i].target = peak(rate, eqFrequencies[i], bounded(p.eq[i], -12, 12, 0));
    filters[10].target = p.filtersEnabled ? pass(rate, bounded(p.highpass, 20, 2000, 20), true)
                                          : std::array<double, 5>{1, 0, 0, 0, 0};
    filters[11].target = p.filtersEnabled ? pass(rate, bounded(p.lowpass, 1000, 20000, 20000), false)
                                          : std::array<double, 5>{1, 0, 0, 0, 0};
    for (auto &f : filters)
        if (initial)
            f.current = f.target;
    const float requestedDelay = bounded(p.predelay, 0, 500, 0) * float(rate / 1000);
    if (initial)
        delayOld = delayNew = requestedDelay;
    else if (requestedDelay != delayNew)
    {
        delayOld += (delayNew - delayOld) * delayBlend;
        delayNew = requestedDelay;
        delayBlend = 0;
    }
    initial = false;
    float inPeak = 0, outPeak = 0;
    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        const int n = std::min(blockSize, buffer.getNumSamples() - offset);
        for (int c = 0; c < channelCount; ++c)
            original.copyFrom(c, 0, buffer, c, offset, n);
        for (int i = 0; i < n; ++i)
        {
            const float gain = smooth[0].getNextValue();
            auto tap = [this](int c, float d) noexcept
            {
                const int whole = int(d);
                const float fraction = d - whole;
                const int size = delay.getNumSamples();
                const int a = (writePosition - whole + size) % size, b = (a - 1 + size) % size;
                return delay.getSample(c, a) * (1 - fraction) + delay.getSample(c, b) * fraction;
            };
            for (int c = 0; c < channelCount; ++c)
            {
                const float raw = original.getSample(c, i);
                inPeak = std::max(inPeak, std::abs(raw));
                const float x = raw * gain;
                dry.setSample(c, i, x);
                delay.setSample(c, writePosition, x);
                wet.setSample(c, i, tap(c, delayOld) * (1 - delayBlend) + tap(c, delayNew) * delayBlend);
            }
            delayBlend = std::min(1.f, delayBlend + float(1 / (rate * 0.02)));
            writePosition = (writePosition + 1) % delay.getNumSamples();
        }
        juce::AudioBuffer<float> wetView(wet.getArrayOfWritePointers(), channelCount, n);
        impulses.processWet(wetView);
        const bool active = impulses.hasActive();
        for (int i = 0; i < n; ++i)
        {
            for (auto &filter : filters)
            {
                filter.step();
                for (int c = 0; c < channelCount; ++c)
                    wet.setSample(c, i, filter.tick(wet.getSample(c, i), c));
            }
            const float width = smooth[4].getNextValue();
            if (channelCount == 2)
            {
                const float mid = (wet.getSample(0, i) + wet.getSample(1, i)) * 0.5f,
                            side = (wet.getSample(0, i) - wet.getSample(1, i)) * 0.5f * width;
                wet.setSample(0, i, mid + side);
                wet.setSample(1, i, mid - side);
            }
            const float outGain = smooth[1].getNextValue(), wetGain = smooth[2].getNextValue(),
                        mix = smooth[3].getNextValue(), bypass = smooth[5].getNextValue();
            for (int c = 0; c < channelCount; ++c)
            {
                const float processed =
                    (dry.getSample(c, i) * (1 - mix) + wet.getSample(c, i) * wetGain * mix) * outGain;
                const float out = active ? processed * (1 - bypass) + original.getSample(c, i) * bypass
                                         : original.getSample(c, i);
                buffer.setSample(c, offset + i, out);
                outPeak = std::max(outPeak, std::abs(out));
            }
        }
    }
    inputPeak.store(inPeak, std::memory_order_relaxed);
    outputPeak.store(outPeak, std::memory_order_relaxed);
}
} // namespace ch
