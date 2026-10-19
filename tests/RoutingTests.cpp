#include "TestSupport.h"
#include "audio/RoutingModel.h"
#include <limits>
TEST(mono_route)
{
    REQUIRE(ch::RoutingModel::diagonal(1, 1).validate(1, 1, 1).wasOk());
}
TEST(stereo_diagonal)
{
    auto m = ch::RoutingModel::diagonal(2, 2);
    REQUIRE(m.validate(2, 2, 2).wasOk());
    REQUIRE(m.routes.size() == 2);
    REQUIRE(m.routes[1].irChannel == 1);
}
TEST(mono_ir_on_stereo)
{
    auto m = ch::RoutingModel::diagonal(2, 1);
    REQUIRE(m.validate(2, 2, 1).wasOk());
    REQUIRE(m.routes[1].irChannel == 0);
}
TEST(true_stereo_explicit)
{
    ch::RoutingModel m{{{0, 0, 0, 1}, {0, 1, 1, 1}, {1, 0, 2, 1}, {1, 1, 3, 1}}};
    REQUIRE(m.validate(2, 2, 4).wasOk());
}
TEST(reject_invalid_routing)
{
    REQUIRE(ch::RoutingModel::diagonal(2, 4).validate(2, 2, 4).failed());
    REQUIRE((ch::RoutingModel{{{2, 0, 0, 1}}}.validate(2, 2, 1).failed()));
    REQUIRE((ch::RoutingModel{{{0, -1, 0, 1}}}.validate(2, 2, 1).failed()));
    REQUIRE((ch::RoutingModel{{{0, 0, 2, 1}}}.validate(2, 2, 1).failed()));
    REQUIRE(
        (ch::RoutingModel{{{0, 0, 0, std::numeric_limits<float>::quiet_NaN()}}}.validate(2, 2, 1).failed()));
}
