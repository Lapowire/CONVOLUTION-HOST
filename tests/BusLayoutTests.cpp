#include "TestSupport.h"
#include "audio/BusLayout.h"
#include "audio/RoutingModel.h"
TEST(host_layout_semantics_are_explicit)
{
    using Set = juce::AudioChannelSet;
    for (auto set :
         {Set::mono(), Set::stereo(), Set::create5point1(), Set::create7point1(), Set::create7point1point2(),
          Set::create7point1point4(), Set::ambisonic(1), Set::ambisonic(2), Set::ambisonic(3)})
        REQUIRE(ch::BusLayout::fromHost(set, ch::BusInterpretation::followHost));
    for (int n = 1; n <= 16; ++n)
        REQUIRE(ch::BusLayout::fromHost(Set::discreteChannels(n), ch::BusInterpretation::followHost));
    REQUIRE(!ch::BusLayout::fromHost(Set::disabled(), ch::BusInterpretation::followHost));
    REQUIRE(!ch::BusLayout::fromHost(Set::discreteChannels(17), ch::BusInterpretation::followHost));
    auto discrete = ch::BusLayout::fromHost(Set::discreteChannels(4), ch::BusInterpretation::followHost);
    auto ambix = ch::BusLayout::fromHost(Set::ambisonic(1), ch::BusInterpretation::followHost);
    REQUIRE(discrete->identity() != ambix->identity());
    REQUIRE(ch::BusLayout::fromHost(Set::discreteChannels(4), ch::BusInterpretation::ambix)->identity() ==
            ambix->identity());
    REQUIRE(!ch::BusLayout::fromHost(Set::create7point1point2(), ch::BusInterpretation::ambix));
    REQUIRE(!ch::BusLayout::fromHost(Set::discreteChannels(8), ch::BusInterpretation::ambix));
    REQUIRE(ambix->stereoPairs.empty());
}
TEST(automatic_routes_preserve_channels_and_exclude_lfe)
{
    using Set = juce::AudioChannelSet;
    for (auto set :
         {Set::create5point1(), Set::create7point1(), Set::create7point1point2(), Set::create7point1point4()})
    {
        auto layout = *ch::BusLayout::fromHost(set, ch::BusInterpretation::followHost);
        REQUIRE(layout.lfe >= 0);
        for (int irChannels : {1, set.size()})
        {
            auto model = ch::RoutingModel::forLayout(layout, irChannels, false);
            REQUIRE(model.routes.size() == std::size_t(set.size() - 1));
            for (auto r : model.routes)
            {
                REQUIRE(r.input == r.output);
                REQUIRE(r.input != layout.lfe);
                REQUIRE(r.irChannel == (irChannels == 1 ? 0 : r.input));
            }
            REQUIRE(model.validate(layout, irChannels));
            model.routes.push_back({layout.lfe, 0, 0, 1});
            REQUIRE(model.validate(layout, irChannels).failed());
            auto enabled = ch::RoutingModel::forLayout(layout, irChannels, true);
            REQUIRE(enabled.routes.size() == std::size_t(set.size()));
            REQUIRE(enabled.validate(layout, irChannels));
        }
        REQUIRE(ch::RoutingModel::forLayout(layout, 3, false).validate(layout, 3).failed());
    }
    ch::RoutingModel automatic;
    ch::RoutingModel silent(std::vector<ch::Route>{});
    REQUIRE(automatic.automatic);
    REQUIRE(!silent.automatic);
}
