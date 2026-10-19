#include "BusLayout.h"
namespace ch
{
std::optional<BusLayout> BusLayout::fromHost(const juce::AudioChannelSet &host, BusInterpretation mode)
{
    using Set = juce::AudioChannelSet;
    if (host.size() < 1 || host.size() > 16)
        return {};
    auto set = host;
    if (mode == BusInterpretation::ambix)
    {
        if (host.isDiscreteLayout())
        {
            int order = host.size() == 4 ? 1 : host.size() == 9 ? 2 : host.size() == 16 ? 3 : 0;
            if (!order)
                return {};
            set = Set::ambisonic(order);
        }
        else if (host.getAmbisonicOrder() < 1 || host.getAmbisonicOrder() > 3)
            return {};
    }
    else if (mode == BusInterpretation::discrete)
    {
        if (!host.isDiscreteLayout())
            return {};
    }
    else if (mode != BusInterpretation::followHost)
        return {};
    const int order = set.getAmbisonicOrder();
    if (!set.isDiscreteLayout() && set != Set::mono() && set != Set::stereo() &&
        set != Set::create5point1() && set != Set::create7point1() && set != Set::create7point1point2() &&
        set != Set::create7point1point4() && !(order >= 1 && order <= 3))
        return {};
    BusLayout result;
    result.channels = set;
    result.interpretation = mode;
    for (int i = 0; i < set.size(); ++i)
    {
        auto type = set.getTypeOfChannel(i);
        result.labels.add(order >= 1               ? "ACN " + juce::String(i)
                          : set.isDiscreteLayout() ? "Channel " + juce::String(i + 1)
                                                   : Set::getAbbreviatedChannelTypeName(type));
        if (type == Set::LFE)
            result.lfe = i;
    }
    if (set == Set::stereo())
        result.stereoPairs.push_back({0, 1});
    return result;
}
juce::String BusLayout::identity() const
{
    using Set = juce::AudioChannelSet;
    if (isAmbix())
        return "ambix-acn-sn3d-" + juce::String(channels.getAmbisonicOrder());
    if (channels.isDiscreteLayout())
        return "discrete-" + juce::String(channels.size());
    if (channels == Set::mono())
        return "mono";
    if (channels == Set::stereo())
        return "stereo";
    if (channels == Set::create5point1())
        return "5.1";
    if (channels == Set::create7point1())
        return "7.1";
    if (channels == Set::create7point1point2())
        return "7.1.2";
    if (channels == Set::create7point1point4())
        return "7.1.4";
    return {};
}
} // namespace ch
