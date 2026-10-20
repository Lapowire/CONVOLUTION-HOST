#pragma once
#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
namespace ch
{
inline constexpr std::array<float, 10> eqFrequencies{31.5f, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
enum class EqType
{
    bell,
    lowShelf,
    highShelf,
    notch
};
struct EqBandShape
{
    float frequencyHz = 1000, q = 1.4f;
    EqType type = EqType::bell;
    bool enabled = true;
    bool operator==(const EqBandShape &) const = default;
};
inline std::array<EqBandShape, 10> defaultEqShapes()
{
    std::array<EqBandShape, 10> result{};
    for (std::size_t i = 0; i < 10; ++i)
        result[i].frequencyHz = eqFrequencies[i];
    return result;
}
struct EqSettings
{
    std::array<float, 10> gainDb{};
    std::array<EqBandShape, 10> bands = defaultEqShapes();
    bool bypass = false;
    bool operator==(const EqSettings &) const = default;
};
class ParametricEq
{
  public:
    void prepare(double sampleRate, int channels);
    void reset() noexcept;
    void setTargets(const EqSettings &) noexcept;
    void process(juce::AudioBuffer<float> &) noexcept;
    static double magnitude(const EqSettings &, double sampleRate, double frequencyHz) noexcept;

  private:
    using Coeff = std::array<double, 5>;
    struct Bank
    {
        Coeff coefficients{1, 0, 0, 0, 0};
        std::array<double, 16> z1{}, z2{};
        float tick(float, int) noexcept;
    };
    struct Band
    {
        Bank current, next;
        double logFrequency = 0, logQ = 0, gain = 0, enabled = 1, blend = 1;
        EqType type = EqType::bell, nextType = EqType::bell;
    };
    static Coeff coefficients(double, double, double, double, EqType) noexcept;
    static EqSettings sanitized(EqSettings) noexcept;
    EqSettings target;
    std::array<Band, 10> bands;
    double rate = 48000, smooth = 0, step = 0, bypass = 0;
    int channels = 2, untilUpdate = 0;
    bool initial = true;
};
} // namespace ch
