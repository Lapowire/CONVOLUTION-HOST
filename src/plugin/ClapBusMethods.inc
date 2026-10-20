// Included inside the pinned CLAP wrapper class by our generated source copy.
bool audioPortsInfo(uint32_t index, bool input, clap_audio_port_info *info) const noexcept override
{
    try
    {
        return ch::clapbus::port(*processor, index, input, info);
    }
    catch (...)
    {
        return false;
    }
}
bool implementsAudioPortsConfig() const noexcept override
{
    return true;
}
uint32_t audioPortsConfigCount() const noexcept override
{
    return ch::clapbus::count;
}
bool audioPortsGetConfig(uint32_t index, clap_audio_ports_config *info) const noexcept override
{
    try
    {
        return ch::clapbus::config(index, info);
    }
    catch (...)
    {
        return false;
    }
}
bool audioPortsSetConfig(clap_id id) noexcept override
{
    if (isActive() || id >= ch::clapbus::count)
        return false;
    try
    {
        auto requested = processor->getBusesLayout();
        auto set = ch::clapbus::layout(id);
        requested.inputBuses.set(0, set);
        requested.outputBuses.set(0, set);
        return processor->setBusesLayout(requested);
    }
    catch (...)
    {
        return false;
    }
}
bool implementsSurround() const noexcept override
{
    return true;
}
bool isChannelMaskSupported(uint64_t mask) const noexcept override
{
    try
    {
        return ch::clapbus::maskSupported(mask);
    }
    catch (...)
    {
        return false;
    }
}
uint32_t getChannelMap(bool input, uint32_t index, uint8_t *map, uint32_t capacity) const noexcept override
{
    if (index != 0)
        return 0;
    return ch::clapbus::channelMap(processor->getChannelLayoutOfBus(input, 0), map, capacity);
}
static ClapJuceWrapper &busSelf(const clap_plugin_t *plugin) noexcept
{
    return static_cast<ClapJuceWrapper &>(Plugin::from(plugin));
}
const void *extension(const char *id) noexcept override
{
    if (std::strcmp(id, CLAP_EXT_AMBISONIC) && std::strcmp(id, CLAP_EXT_AMBISONIC_COMPAT))
        return nullptr;
    static const clap_plugin_ambisonic_t ambisonic{
        [](const clap_plugin_t *, const clap_ambisonic_config_t *config) -> bool
        {
            return config && config->ordering == CLAP_AMBISONIC_ORDERING_ACN &&
                   config->normalization == CLAP_AMBISONIC_NORMALIZATION_SN3D;
        },
        [](const clap_plugin_t *plugin, bool input, uint32_t index, clap_ambisonic_config_t *config) -> bool
        {
            if (!config || index != 0 ||
                busSelf(plugin).processor->getChannelLayoutOfBus(input, 0).getAmbisonicOrder() < 1)
                return false;
            *config = {CLAP_AMBISONIC_ORDERING_ACN, CLAP_AMBISONIC_NORMALIZATION_SN3D};
            return true;
        }};
    return &ambisonic;
}
