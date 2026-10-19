#pragma once
#include <juce_core/juce_core.h>
#include <vector>
namespace ch
{
struct Route
{
    int input = 0, output = 0, irChannel = 0;
    float gain = 1.0f;
};
struct RoutingModel
{
    std::vector<Route> routes;
    juce::Result validate(int inputs, int outputs, int irChannels) const;
    static RoutingModel diagonal(int channels, int irChannels);
};
} // namespace ch
