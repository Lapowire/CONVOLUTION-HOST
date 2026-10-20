#include "ParametricEq.h"
#include <cmath>
#include <complex>
namespace ch
{
namespace
{
double bound(double v, double lo, double hi, double fallback) noexcept
{
    return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
}
} // namespace
ParametricEq::Coeff ParametricEq::coefficients(double sr, double f, double q, double gain,
                                               EqType type) noexcept
{
    if (f >= sr * 0.5 || (type != EqType::notch && std::abs(gain) < 1e-9))
        return {1, 0, 0, 0, 0};
    const double w = 2 * juce::MathConstants<double>::pi * f / sr, c = std::cos(w), a = std::sin(w) / (2 * q),
                 A = std::pow(10, gain / 40), b = 2 * std::sqrt(A) * a;
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (type)
    {
    case EqType::bell:
        b0 = 1 + a * A;
        b1 = -2 * c;
        b2 = 1 - a * A;
        a0 = 1 + a / A;
        a1 = -2 * c;
        a2 = 1 - a / A;
        break;
    case EqType::notch:
        b0 = 1;
        b1 = -2 * c;
        b2 = 1;
        a0 = 1 + a;
        a1 = -2 * c;
        a2 = 1 - a;
        break;
    case EqType::lowShelf:
        b0 = A * ((A + 1) - (A - 1) * c + b);
        b1 = 2 * A * ((A - 1) - (A + 1) * c);
        b2 = A * ((A + 1) - (A - 1) * c - b);
        a0 = (A + 1) + (A - 1) * c + b;
        a1 = -2 * ((A - 1) + (A + 1) * c);
        a2 = (A + 1) + (A - 1) * c - b;
        break;
    case EqType::highShelf:
        b0 = A * ((A + 1) + (A - 1) * c + b);
        b1 = -2 * A * ((A - 1) + (A + 1) * c);
        b2 = A * ((A + 1) + (A - 1) * c - b);
        a0 = (A + 1) - (A - 1) * c + b;
        a1 = 2 * ((A - 1) - (A + 1) * c);
        a2 = (A + 1) - (A - 1) * c - b;
        break;
    }
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
EqSettings ParametricEq::sanitized(EqSettings s) noexcept
{
    for (std::size_t i = 0; i < 10; ++i)
    {
        s.gainDb[i] = float(bound(s.gainDb[i], -12, 12, 0));
        s.bands[i].frequencyHz = float(bound(s.bands[i].frequencyHz, 20, 20000, eqFrequencies[i]));
        s.bands[i].q = float(bound(s.bands[i].q, 0.1, 18, 1.4));
        if (int(s.bands[i].type) < 0 || int(s.bands[i].type) > 3)
            s.bands[i].type = EqType::bell;
    }
    return s;
}
void ParametricEq::prepare(double sr, int n)
{
    rate = bound(sr, 8000, 384000, 48000);
    channels = juce::jlimit(1, 16, n);
    smooth = 1 - std::exp(-16 / (rate * 0.02));
    step = 1 / (rate * 0.02);
    reset();
}
void ParametricEq::reset() noexcept
{
    bands = {};
    initial = true;
    untilUpdate = 0;
}
void ParametricEq::setTargets(const EqSettings &s) noexcept
{
    target = sanitized(s);
}
float ParametricEq::Bank::tick(float x, int ch) noexcept
{
    auto i = std::size_t(ch);
    const auto &c = coefficients;
    double y = c[0] * x + z1[i];
    z1[i] = c[1] * x - c[3] * y + z2[i];
    z2[i] = c[2] * x - c[4] * y;
    return float(y);
}
void ParametricEq::process(juce::AudioBuffer<float> &audio) noexcept
{
    if (audio.getNumChannels() != channels)
        return;
    if (initial)
    {
        bypass = target.bypass ? 1 : 0;
        for (std::size_t j = 0; j < 10; ++j)
        {
            auto &b = bands[j];
            const auto &s = target.bands[j];
            b.logFrequency = std::log(s.frequencyHz);
            b.logQ = std::log(s.q);
            b.gain = target.gainDb[j];
            b.type = b.nextType = s.type;
            b.enabled = s.enabled ? 1 : 0;
            b.blend = 1;
        }
        initial = false;
    }
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        if (untilUpdate-- <= 0)
        {
            untilUpdate = 15;
            for (std::size_t j = 0; j < 10; ++j)
            {
                auto &b = bands[j];
                const auto &t = target.bands[j];
                b.logFrequency += (std::log(t.frequencyHz) - b.logFrequency) * smooth;
                b.logQ += (std::log(t.q) - b.logQ) * smooth;
                b.gain += (target.gainDb[j] - b.gain) * smooth;
                if (b.blend >= 1 && b.type != t.type)
                {
                    b.next = {};
                    b.nextType = t.type;
                    b.blend = 0;
                }
                b.current.coefficients =
                    coefficients(rate, std::exp(b.logFrequency), std::exp(b.logQ), b.gain, b.type);
                b.next.coefficients =
                    coefficients(rate, std::exp(b.logFrequency), std::exp(b.logQ), b.gain, b.nextType);
            }
        }
        bypass += juce::jlimit(-step, step, (target.bypass ? 1. : 0.) - bypass);
        std::array<float, 16> original{};
        for (int c = 0; c < channels; ++c)
            original[std::size_t(c)] = audio.getSample(c, i);
        for (std::size_t j = 0; j < 10; ++j)
        {
            auto &b = bands[j];
            b.enabled += juce::jlimit(-step, step, (target.bands[j].enabled ? 1. : 0.) - b.enabled);
            const bool switching = b.blend < 1;
            for (int c = 0; c < channels; ++c)
            {
                float x = audio.getSample(c, i), y = b.current.tick(x, c);
                if (switching)
                    y = float(y * (1 - b.blend) + b.next.tick(x, c) * b.blend);
                audio.setSample(c, i, float(x + (y - x) * b.enabled));
            }
            if (switching)
            {
                b.blend = std::min(1., b.blend + step);
                if (b.blend >= 1)
                {
                    b.current = b.next;
                    b.type = b.nextType;
                }
            }
        }
        for (int c = 0; c < channels; ++c)
            audio.setSample(c, i,
                            float(audio.getSample(c, i) * (1 - bypass) + original[std::size_t(c)] * bypass));
    }
}
double ParametricEq::magnitude(const EqSettings &input, double sr, double f) noexcept
{
    auto s = sanitized(input);
    if (s.bypass)
        return 1;
    if (!std::isfinite(sr) || sr < 8000 || !std::isfinite(f) || f < 0)
        return 1;
    auto z = std::polar(1., -2 * juce::MathConstants<double>::pi * f / sr);
    double response = 1;
    for (std::size_t i = 0; i < 10; ++i)
        if (s.bands[i].enabled)
        {
            auto c = coefficients(sr, s.bands[i].frequencyHz, s.bands[i].q, s.gainDb[i], s.bands[i].type);
            response *= std::abs((c[0] + c[1] * z + c[2] * z * z) / (1. + c[3] * z + c[4] * z * z));
        }
    return response;
}
} // namespace ch
