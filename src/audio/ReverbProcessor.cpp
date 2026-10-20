#include "ReverbProcessor.h"
#include <cmath>
#include <stdexcept>
namespace ch
{
namespace
{
float bounded(float v, float lo, float hi, float fallback)
{
    return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
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
    widthPairCount = channels == 2 ? 1 : 0;
    widthPairs[0] = {0, 1};
    wet.setSize(channels, blockSize);
    dry.setSize(channels, blockSize);
    original.setSize(channels, blockSize);
    delay.setSize(channels, int(sr * 0.5) + 2);
    delay.clear();
    writePosition = 0;
    delayOld = delayNew = delayDesired = 0;
    delayBlend = 1;
    initial = true;
    for (auto &s : smooth)
        s.reset(sr, 0.02);
    for (auto &f : filters)
    {
        f = Filter{};
        f.smoothing = 1 - std::exp(-1 / (sr * 0.02));
    }
    parametricEq.prepare(sr, channels);
    impulses.configure(sr, blockSize, channels);
    inputPeak.store(0);
    outputPeak.store(0);
}
void ReverbProcessor::configureWidthPairs(std::span<const StereoPair> pairs)
{
    if (pairs.size() > widthPairs.size())
        throw std::invalid_argument("Too many stereo pairs");
    std::array<bool, 16> used{};
    for (auto pair : pairs)
    {
        if (pair.left < 0 || pair.right < 0 || pair.left >= channelCount || pair.right >= channelCount ||
            pair.left == pair.right || used[std::size_t(pair.left)] || used[std::size_t(pair.right)])
            throw std::invalid_argument("Stereo pairs must be in range and nonoverlapping");
        used[std::size_t(pair.left)] = used[std::size_t(pair.right)] = true;
    }
    std::copy(pairs.begin(), pairs.end(), widthPairs.begin());
    widthPairCount = int(pairs.size());
}
void ReverbProcessor::reset() noexcept
{
    impulses.reset();
    parametricEq.reset();
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
    parametricEq.setTargets({p.eq, p.eqShape, p.eqBypass});
    filters[0].target = p.filtersEnabled ? pass(rate, bounded(p.highpass, 20, 2000, 20), true)
                                         : std::array<double, 5>{1, 0, 0, 0, 0};
    filters[1].target = p.filtersEnabled ? pass(rate, bounded(p.lowpass, 1000, 20000, 20000), false)
                                         : std::array<double, 5>{1, 0, 0, 0, 0};
    for (auto &f : filters)
        if (initial)
            f.current = f.target;
    delayDesired = bounded(p.predelay, 0, 500, 0) * float(rate / 1000);
    if (initial)
        delayOld = delayNew = delayDesired;
    initial = false;
    float inPeak = 0, outPeak = 0;
    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        const int n = std::min(blockSize, buffer.getNumSamples() - offset);
        for (int c = 0; c < channelCount; ++c)
            original.copyFrom(c, 0, buffer, c, offset, n);
        for (int i = 0; i < n; ++i)
        {
            // Finish the current signal crossfade before starting the latest queued target.
            // A weighted average of delay positions is not a blend of the delayed signals.
            if (delayBlend >= 1 && delayDesired != delayNew)
            {
                delayOld = delayNew;
                delayNew = delayDesired;
                delayBlend = 0;
            }
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
        parametricEq.process(wetView);
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
            for (int pairIndex = 0; pairIndex < widthPairCount; ++pairIndex)
            {
                auto pair = widthPairs[std::size_t(pairIndex)];
                const float mid = (wet.getSample(pair.left, i) + wet.getSample(pair.right, i)) * 0.5f,
                            side =
                                (wet.getSample(pair.left, i) - wet.getSample(pair.right, i)) * 0.5f * width;
                wet.setSample(pair.left, i, mid + side);
                wet.setSample(pair.right, i, mid - side);
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
