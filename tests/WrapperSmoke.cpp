#include "BuildIdentity.h"
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <stdexcept>
#if CH_SMOKE_CLAP
#include <clap/clap.h>
#include <clap/ext/ambisonic.h>
#include <clap/ext/surround.h>
#include <vector>
#endif
namespace
{
void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
void verifyIdentity(const void *data, std::size_t size)
{
    const std::string bytes(static_cast<const char *>(data), size);
    require(bytes.find(CH_BUILD_ID) != std::string::npos, "Module build identity differs from test runner");
}
void vst3(const juce::String &path)
{
    juce::AudioPluginFormatManager manager;
    manager.addFormat(new juce::VST3PluginFormat);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    manager.getFormat(0)->findAllTypesForFile(descriptions, path);
    require(!descriptions.isEmpty(), "No VST3 plugin descriptions");
    juce::String error;
    auto instance = manager.createPluginInstance(*descriptions[0], 48000, 256, error);
    require(bool(instance), error.toRawUTF8());
    require(!instance->getParameters().isEmpty(), "No VST3 parameters");
    juce::AudioProcessorParameter *frequency = nullptr;
    bool dampingFound = false;
    for (auto *parameter : instance->getParameters())
    {
        if (parameter->getName(128) == "EQ 6 frequency")
            frequency = parameter;
        if (parameter->getName(128) == "Damping 4 dB per second")
            dampingFound = true;
    }
    require(frequency && dampingFound, "VST3 parametric EQ/damping parameters missing");
    frequency->setValueNotifyingHost(0.42f);
    juce::MemoryBlock state;
    instance->getStateInformation(state);
    require(state.getSize() > 0, "No VST3 state");
    auto envelope = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
    require(envelope && envelope->hasTagName("VST3PluginState"), "VST3 host state envelope missing");
    auto *component = envelope->getChildByName("IComponent");
    juce::MemoryBlock componentState;
    require(component && componentState.fromBase64Encoding(component->getAllSubText()),
            "VST3 component state missing");
    verifyIdentity(componentState.getData(), componentState.getSize());
    frequency->setValueNotifyingHost(0.1f);
    instance->setStateInformation(state.getData(), int(state.getSize()));
    require(std::abs(frequency->getValue() - 0.42f) < 0.001f, "VST3 new parameter state restore failed");
    using Set = juce::AudioChannelSet;
    const std::array<Set, 9> layouts{Set::mono(),
                                     Set::stereo(),
                                     Set::create5point1(),
                                     Set::create7point1(),
                                     Set::create7point1point2(),
                                     Set::create7point1point4(),
                                     Set::ambisonic(1),
                                     Set::ambisonic(2),
                                     Set::ambisonic(3)};
    for (const auto &channels : layouts)
    {
        auto buses = instance->getBusesLayout();
        buses.inputBuses.set(0, channels);
        buses.outputBuses.set(0, channels);
        require(instance->setBusesLayout(buses),
                ("VST3 rejected bus: " + channels.getDescription()).toRawUTF8());
        instance->prepareToPlay(48000, 256);
        juce::AudioBuffer<float> audio(channels.size(), 256);
        audio.clear();
        for (int c = 0; c < channels.size(); ++c)
            audio.setSample(c, 0, float(c + 1) / 100);
        juce::MidiBuffer midi;
        instance->processBlock(audio, midi);
        for (int c = 0; c < channels.size(); ++c)
        {
            require(std::abs(audio.getSample(c, 0) - float(c + 1) / 100) < 1e-5f,
                    "VST3 channel passthrough failed");
            for (int i = 0; i < 256; ++i)
                require(std::isfinite(audio.getSample(c, i)), "Nonfinite VST3 output");
        }
        instance->releaseResources();
    }
    std::cout << "Verified 9 native VST3 mono/stereo/surround/AmbiX configurations\n";
}
#if CH_SMOKE_CLAP
struct Memory
{
    std::vector<unsigned char> bytes;
    std::size_t pos = 0;
};
int64_t CLAP_ABI writeState(const clap_ostream_t *stream, const void *data, uint64_t n)
{
    auto &m = *static_cast<Memory *>(stream->ctx);
    if (n > 4 * 1024 * 1024 - m.bytes.size())
        return -1;
    auto *p = static_cast<const unsigned char *>(data);
    m.bytes.insert(m.bytes.end(), p, p + n);
    return int64_t(n);
}
int64_t CLAP_ABI readState(const clap_istream_t *stream, void *data, uint64_t n)
{
    auto &m = *static_cast<Memory *>(stream->ctx);
    auto size = std::min(std::size_t(n), m.bytes.size() - m.pos);
    std::memcpy(data, m.bytes.data() + m.pos, size);
    m.pos += size;
    return int64_t(size);
}
const void *CLAP_ABI extension(const clap_host_t *, const char *)
{
    return nullptr;
}
void CLAP_ABI request(const clap_host_t *) {}
uint32_t CLAP_ABI eventCount(const clap_input_events_t *)
{
    return 0;
}
const clap_event_header_t *CLAP_ABI event(const clap_input_events_t *, uint32_t)
{
    return nullptr;
}
bool CLAP_ABI push(const clap_output_events_t *, const clap_event_header_t *)
{
    return false;
}
struct ClapOwner
{
    const clap_plugin_t *plugin = nullptr;
    const clap_plugin_entry_t *entry = nullptr;
    bool active = false, processing = false;
    ~ClapOwner()
    {
        if (plugin)
        {
            if (processing)
                plugin->stop_processing(plugin);
            if (active)
                plugin->deactivate(plugin);
            plugin->destroy(plugin);
        }
        if (entry)
            entry->deinit();
    }
};
void clap(const juce::String &path)
{
    juce::DynamicLibrary library;
    require(library.open(path), "Could not load CLAP module");
    auto *entry = static_cast<const clap_plugin_entry_t *>(library.getFunction("clap_entry"));
    require(entry && clap_version_is_compatible(entry->clap_version), "Missing/incompatible clap_entry");
    require(entry->init(path.toRawUTF8()), "CLAP entry init failed");
    ClapOwner owner;
    owner.entry = entry;
    auto *factory = static_cast<const clap_plugin_factory_t *>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    require(factory && factory->get_plugin_count(factory) > 0, "Missing CLAP factory");
    auto *descriptor = factory->get_plugin_descriptor(factory, 0);
    require(descriptor && std::string(descriptor->id) == "com.lapowire.convolutionhost", "Wrong CLAP ID");
    clap_host_t host{CLAP_VERSION,
                     nullptr,
                     "Convolution Host smoke test",
                     "Lapowire",
                     "https://github.com/Lapowire/CONVOLUTION-HOST",
                     "1",
                     extension,
                     request,
                     request,
                     request};
    owner.plugin = factory->create_plugin(factory, &host, descriptor->id);
    auto *p = owner.plugin;
    require(p && p->init(p), "CLAP instance init failed");
    auto *ports = static_cast<const clap_plugin_audio_ports_t *>(p->get_extension(p, CLAP_EXT_AUDIO_PORTS));
    require(ports && ports->count(p, true) == 1 && ports->count(p, false) == 1, "CLAP buses missing");
    clap_audio_port_info_t in{}, out{};
    require(ports->get(p, 0, true, &in) && ports->get(p, 0, false, &out), "CLAP port info missing");
    require(in.channel_count == out.channel_count && in.channel_count <= 16 && in.channel_count > 0,
            "CLAP channel count invalid");
    auto *configurations = static_cast<const clap_plugin_audio_ports_config_t *>(
        p->get_extension(p, CLAP_EXT_AUDIO_PORTS_CONFIG));
    require(configurations && configurations->count(p) == 25,
            "CLAP mono, discrete, surround and AmbiX configurations missing");
    auto *parameters = static_cast<const clap_plugin_params_t *>(p->get_extension(p, CLAP_EXT_PARAMS));
    require(parameters && parameters->count(p) > 0, "CLAP parameters missing");
    auto *state = static_cast<const clap_plugin_state_t *>(p->get_extension(p, CLAP_EXT_STATE));
    require(state, "CLAP state missing");
    bool frequencyFound = false, dampingFound = false;
    for (uint32_t i = 0; i < parameters->count(p); ++i)
    {
        clap_param_info_t info{};
        require(parameters->get_info(p, i, &info), "CLAP parameter info missing");
        if (std::string(info.name) == "EQ 6 frequency")
            frequencyFound = true;
        if (std::string(info.name) == "Damping 4 dB per second")
            dampingFound = true;
    }
    require(frequencyFound && dampingFound, "CLAP parametric EQ/damping parameters missing");
    Memory memory;
    clap_ostream_t outputStream{&memory, writeState};
    require(state->save(p, &outputStream) && !memory.bytes.empty(), "CLAP save failed");
    verifyIdentity(memory.bytes.data(), memory.bytes.size());
    require(std::string(reinterpret_cast<const char *>(memory.bytes.data()), memory.bytes.size())
                    .find("peq5Freq") != std::string::npos,
            "CLAP state lacks new EQ parameters");
    clap_istream_t inputStream{&memory, readState};
    require(state->load(p, &inputStream), "CLAP restore failed");
    require(!configurations->select(p, CLAP_INVALID_ID), "Invalid CLAP bus id accepted");
    for (uint32_t index = 0; index < configurations->count(p); ++index)
    {
        clap_audio_ports_config_t configuration{};
        require(configurations->get(p, index, &configuration), "CLAP bus description missing");
        require(configurations->select(p, configuration.id), "CLAP bus configuration rejected");
        require(ports->get(p, 0, true, &in) && ports->get(p, 0, false, &out), "CLAP selected port missing");
        require(in.channel_count == configuration.main_input_channel_count &&
                    out.channel_count == in.channel_count,
                "CLAP current ports did not change with selection");
        require((!in.port_type && !configuration.main_input_port_type) ||
                    (in.port_type && configuration.main_input_port_type &&
                     !std::strcmp(in.port_type, configuration.main_input_port_type)),
                "CLAP selected port type differs from configuration");
        if (index >= 18 && index <= 21)
        {
            auto *surround =
                static_cast<const clap_plugin_surround_t *>(p->get_extension(p, CLAP_EXT_SURROUND));
            require(surround, "CLAP surround channel metadata missing");
            std::array<uint8_t, 16> map{}, outputMap{};
            require(surround->get_channel_map(p, true, 0, map.data(), 16) == in.channel_count,
                    "CLAP surround input map missing");
            require(surround->get_channel_map(p, false, 0, outputMap.data(), 16) == in.channel_count &&
                        map == outputMap,
                    "CLAP surround input/output maps differ");
            const std::vector<std::vector<uint8_t>> expected{{0, 1, 2, 3, 9, 10},
                                                             {0, 1, 2, 3, 9, 10, 4, 5},
                                                             {0, 1, 2, 3, 9, 10, 4, 5, 18, 19},
                                                             {0, 1, 2, 3, 9, 10, 12, 14, 15, 17, 4, 5}};
            require(std::equal(expected[index - 18].begin(), expected[index - 18].end(), map.begin()),
                    "CLAP surround speaker order differs from JUCE bus");
            uint64_t mask = 0;
            for (uint32_t c = 0; c < in.channel_count; ++c)
                mask |= uint64_t(1) << map[c];
            require(surround->is_channel_mask_supported(p, mask),
                    "CLAP reported surround mask is unsupported");
        }
        if (index >= 22)
        {
            auto *ambisonic =
                static_cast<const clap_plugin_ambisonic_t *>(p->get_extension(p, CLAP_EXT_AMBISONIC));
            clap_ambisonic_config_t convention{};
            require(ambisonic && ambisonic->get_config(p, true, 0, &convention),
                    "CLAP AmbiX metadata missing");
            require(convention.ordering == CLAP_AMBISONIC_ORDERING_ACN &&
                        convention.normalization == CLAP_AMBISONIC_NORMALIZATION_SN3D,
                    "CLAP AmbiX convention changed");
            require(ambisonic->is_config_supported(p, &convention), "CLAP AmbiX convention rejected");
            convention.normalization = CLAP_AMBISONIC_NORMALIZATION_N3D;
            require(!ambisonic->is_config_supported(p, &convention),
                    "CLAP silently accepts another normalization");
        }
        require(p->activate(p, 48000, 64, 256), "CLAP activate failed");
        owner.active = true;
        require(p->start_processing(p), "CLAP processing start failed");
        owner.processing = true;
        float input[16][256]{}, output[16][256]{};
        float *inputPointers[16]{}, *outputPointers[16]{};
        for (int c = 0; c < 16; ++c)
        {
            inputPointers[c] = input[c];
            outputPointers[c] = output[c];
        }
        for (uint32_t c = 0; c < in.channel_count; ++c)
            input[c][0] = float(c + 1) / 100;
        clap_audio_buffer_t inputBuffer{inputPointers, nullptr, in.channel_count, 0, 0},
            outputBuffer{outputPointers, nullptr, out.channel_count, 0, 0};
        clap_input_events_t inputEvents{nullptr, eventCount, event};
        clap_output_events_t outputEvents{nullptr, push};
        clap_process_t process{};
        process.frames_count = 256;
        process.audio_inputs = &inputBuffer;
        process.audio_outputs = &outputBuffer;
        process.audio_inputs_count = process.audio_outputs_count = 1;
        process.in_events = &inputEvents;
        process.out_events = &outputEvents;
        require(p->process(p, &process) != CLAP_PROCESS_ERROR, "CLAP processing failed");
        for (uint32_t c = 0; c < out.channel_count; ++c)
            require(std::abs(output[c][0] - input[c][0]) < 1e-5f, "CLAP channel passthrough failed");
        for (uint32_t c = 0; c < out.channel_count; ++c)
            for (float sample : output[c])
                require(std::isfinite(sample), "Nonfinite CLAP output");
        p->stop_processing(p);
        owner.processing = false;
        p->deactivate(p);
        owner.active = false;
    }
    std::cout << "Verified 25 CLAP bus configurations and channel metadata\n";
}
#endif
} // namespace
int main(int argc, char **argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        require(argc == 5 && std::string(argv[1]) == "--format" && std::string(argv[3]) == "--plugin",
                "Usage: ch_wrapper_smoke --format vst3|clap --plugin path");
        juce::String path = argv[4];
        require(juce::File(path).exists(), "Plugin path does not exist");
        if (std::string(argv[2]) == "vst3")
            vst3(path);
#if CH_SMOKE_CLAP
        else if (std::string(argv[2]) == "clap")
            clap(path);
#endif
        else
            throw std::runtime_error("Unknown format");
        std::cout << "Build identity: " << CH_BUILD_ID << std::endl;
        std::cout << "PASS " << argv[2] << " native module: factory, instance, parameters, state and audio\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
