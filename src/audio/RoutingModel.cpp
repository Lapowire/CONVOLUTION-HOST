#include "RoutingModel.h"
#include <cmath>
namespace ch
{
juce::Result RoutingModel::validate(int inputs, int outputs, int irChannels) const
{
    if (inputs < 1 || outputs < 1 || inputs > 16 || outputs > 16 || irChannels < 1 || irChannels > 16 ||
        routes.empty() || routes.size() > 256)
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
    for (int c = 0; c < channels; ++c)
        m.routes.push_back({c, c, irChannels == 1 ? 0 : c, 1});
    return m;
}
} // namespace ch
