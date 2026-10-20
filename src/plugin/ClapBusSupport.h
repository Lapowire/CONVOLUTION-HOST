#pragma once
#include <array>
#include <clap/ext/ambisonic.h>
#include <clap/ext/audio-ports-config.h>
#include <clap/ext/surround.h>
#include <cstdio>
#include <cstring>
#include <juce_audio_processors/juce_audio_processors.h>
namespace ch::clapbus
{
inline constexpr uint32_t count = 25;
inline juce::AudioChannelSet layout(uint32_t index)
{
    using Set = juce::AudioChannelSet;
    if (index == 0)
        return Set::mono();
    if (index == 1)
        return Set::stereo();
    if (index >= 2 && index < 18)
        return Set::discreteChannels(int(index - 1));
    switch (index)
    {
    case 18:
        return Set::create5point1();
    case 19:
        return Set::create7point1();
    case 20:
        return Set::create7point1point2();
    case 21:
        return Set::create7point1point4();
    case 22:
        return Set::ambisonic(1);
    case 23:
        return Set::ambisonic(2);
    case 24:
        return Set::ambisonic(3);
    default:
        return Set::disabled();
    }
}
inline const char *portType(const juce::AudioChannelSet &set)
{
    if (set == juce::AudioChannelSet::mono())
        return CLAP_PORT_MONO;
    if (set == juce::AudioChannelSet::stereo())
        return CLAP_PORT_STEREO;
    if (set.getAmbisonicOrder() >= 1)
        return CLAP_PORT_AMBISONIC;
    return set.isDiscreteLayout() ? nullptr : CLAP_PORT_SURROUND;
}
inline bool config(uint32_t index, clap_audio_ports_config_t *info)
{
    if (!info || index >= count)
        return false;
    const auto set = layout(index);
    *info = {};
    info->id = index;
    auto name = set.getDescription();
    if (set.getAmbisonicOrder() >= 1)
        name += " ACN / SN3D";
    std::snprintf(info->name, sizeof(info->name), "%s", name.toRawUTF8());
    info->input_port_count = info->output_port_count = 1;
    info->has_main_input = info->has_main_output = true;
    info->main_input_channel_count = info->main_output_channel_count = uint32_t(set.size());
    info->main_input_port_type = info->main_output_port_type = portType(set);
    return true;
}
inline bool port(const juce::AudioProcessor &processor, uint32_t index, bool input,
                 clap_audio_port_info_t *info)
{
    if (!info || index != 0 || processor.getBusCount(input) != 1)
        return false;
    const auto *bus = processor.getBus(input, 0);
    const auto &set = bus->getCurrentLayout();
    *info = {};
    info->id = input ? 1 << 15 : 1;
    info->in_place_pair = input ? 1 : 1 << 15;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    std::snprintf(info->name, sizeof(info->name), "%s", bus->getName().toRawUTF8());
    info->channel_count = uint32_t(set.size());
    info->port_type = portType(set);
    return true;
}
inline uint8_t speaker(juce::AudioChannelSet::ChannelType channel)
{
    using Set = juce::AudioChannelSet;
    switch (channel)
    {
    case Set::left:
        return CLAP_SURROUND_FL;
    case Set::right:
        return CLAP_SURROUND_FR;
    case Set::centre:
        return CLAP_SURROUND_FC;
    case Set::LFE:
        return CLAP_SURROUND_LFE;
    case Set::leftSurround:
    case Set::leftSurroundSide:
        return CLAP_SURROUND_SL;
    case Set::rightSurround:
    case Set::rightSurroundSide:
        return CLAP_SURROUND_SR;
    case Set::leftSurroundRear:
        return CLAP_SURROUND_BL;
    case Set::rightSurroundRear:
        return CLAP_SURROUND_BR;
    case Set::topFrontLeft:
        return CLAP_SURROUND_TFL;
    case Set::topFrontRight:
        return CLAP_SURROUND_TFR;
    case Set::topRearLeft:
        return CLAP_SURROUND_TBL;
    case Set::topRearRight:
        return CLAP_SURROUND_TBR;
    case Set::topSideLeft:
        return CLAP_SURROUND_TSL;
    case Set::topSideRight:
        return CLAP_SURROUND_TSR;
    default:
        return 255;
    }
}
inline uint32_t channelMap(const juce::AudioChannelSet &set, uint8_t *map, uint32_t capacity)
{
    if (!map || capacity < uint32_t(set.size()) || set.size() <= 2 || set.isDiscreteLayout() ||
        set.getAmbisonicOrder() >= 1)
        return 0;
    for (int c = 0; c < set.size(); ++c)
    {
        auto id = speaker(set.getTypeOfChannel(c));
        if (id == 255)
            return 0;
        map[c] = id;
    }
    return uint32_t(set.size());
}
inline bool maskSupported(uint64_t mask)
{
    for (uint32_t index = 18; index <= 21; ++index)
    {
        std::array<uint8_t, 16> map{};
        auto n = channelMap(layout(index), map.data(), uint32_t(map.size()));
        uint64_t candidate = 0;
        for (uint32_t c = 0; c < n; ++c)
            candidate |= uint64_t(1) << map[c];
        if (mask == candidate)
            return true;
    }
    return false;
}
} // namespace ch::clapbus
