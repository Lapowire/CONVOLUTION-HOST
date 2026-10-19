#pragma once
#include "BusLayout.h"
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
    RoutingModel() = default;
    RoutingModel(std::vector<Route> paths) : routes(std::move(paths)), automatic(false) {}
    std::vector<Route> routes;
    bool automatic = true, lfeWetEnabled = false;
    juce::String sourceLayout;
    juce::Result validate(const BusLayout &, int irChannels) const;
    static RoutingModel forLayout(const BusLayout &, int irChannels, bool lfeWetEnabled);
    juce::Result validate(int inputs, int outputs, int irChannels) const;
    static RoutingModel diagonal(int channels, int irChannels);
};
} // namespace ch
