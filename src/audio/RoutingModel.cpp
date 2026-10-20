#include "RoutingModel.h"
#include <cmath>
namespace ch
{
juce::Result RoutingModel::validate(int inputs, int outputs, int irChannels) const
{
    if (inputs < 1 || outputs < 1 || inputs > 16 || outputs > 16 || irChannels < 1 || irChannels > 16 ||
        (routes.empty() && automatic) || routes.size() > 256)
        return juce::Result::fail("Routing requires 1-16 channels and 1-256 explicit paths");
    for (const auto &r : routes)
        if (r.input < 0 || r.input >= inputs || r.output < 0 || r.output >= outputs || r.irChannel < 0 ||
            r.irChannel >= irChannels || !std::isfinite(r.gain) || std::abs(r.gain) > 16)
            return juce::Result::fail("Invalid routing channel or gain");
    return juce::Result::ok();
}
RoutingModel RoutingModel::diagonal(int channels, int irChannels)
{
    RoutingModel m;
    if (channels < 1 || channels > 16 || (irChannels != 1 && irChannels != channels))
        return m;
    m.automatic = false;
    for (int c = 0; c < channels; ++c)
        m.routes.push_back({c, c, irChannels == 1 ? 0 : c, 1});
    return m;
}
juce::Result RoutingModel::validate(const BusLayout &layout, int irChannels) const
{
    auto basic = validate(layout.channels.size(), layout.channels.size(), irChannels);
    if (basic.failed())
        return basic;
    if (sourceLayout.isNotEmpty() && sourceLayout != layout.identity() &&
        !(sourceLayout == "legacy-stereo" && (layout.identity() == "mono" || layout.identity() == "stereo")))
        return juce::Result::fail("Saved routing belongs to a different bus layout");
    for (auto route : routes)
        if (!lfeWetEnabled && layout.lfe >= 0 && (route.input == layout.lfe || route.output == layout.lfe))
            return juce::Result::fail("Enable LFE wet explicitly before routing the LFE channel");
    return juce::Result::ok();
}
RoutingModel RoutingModel::forLayout(const BusLayout &layout, int irChannels, bool enableLfe)
{
    auto result = diagonal(layout.channels.size(), irChannels);
    result.sourceLayout = layout.identity();
    result.lfeWetEnabled = enableLfe;
    if (!enableLfe && layout.lfe >= 0)
        std::erase_if(result.routes, [&](auto r) { return r.input == layout.lfe || r.output == layout.lfe; });
    return result;
}
} // namespace ch
