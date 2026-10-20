#include "BuildIdentity.h"
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <stdexcept>
#if CH_SMOKE_CLAP
#include <clap/clap.h>
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
    instance->setStateInformation(state.getData(), int(state.getSize()));
    instance->prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(
        std::max(instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels()), 256);
    audio.clear();
    audio.setSample(0, 0, 0.25f);
    juce::MidiBuffer midi;
    instance->processBlock(audio, midi);
    require(std::abs(audio.getSample(0, 0) - 0.25f) < 1e-5f, "VST3 unity passthrough failed");
    for (int c = 0; c < audio.getNumChannels(); ++c)
        for (int i = 0; i < 256; ++i)
            require(std::isfinite(audio.getSample(c, i)), "Nonfinite VST3 output");
    instance->releaseResources();
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
    auto *parameters = static_cast<const clap_plugin_params_t *>(p->get_extension(p, CLAP_EXT_PARAMS));
    require(parameters && parameters->count(p) > 0, "CLAP parameters missing");
    auto *state = static_cast<const clap_plugin_state_t *>(p->get_extension(p, CLAP_EXT_STATE));
    require(state, "CLAP state missing");
    Memory memory;
    clap_ostream_t outputStream{&memory, writeState};
    require(state->save(p, &outputStream) && !memory.bytes.empty(), "CLAP save failed");
    verifyIdentity(memory.bytes.data(), memory.bytes.size());
    clap_istream_t inputStream{&memory, readState};
    require(state->load(p, &inputStream), "CLAP restore failed");
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
    input[0][0] = 0.25f;
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
    require(std::abs(output[0][0] - 0.25f) < 1e-5f, "CLAP unity passthrough failed");
    for (uint32_t c = 0; c < out.channel_count; ++c)
        for (float sample : output[c])
            require(std::isfinite(sample), "Nonfinite CLAP output");
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
