#include "StateStore.h"
#include <cmath>
#include <juce_cryptography/juce_cryptography.h>
namespace ch
{
juce::ValueTree StateStore::encode(const SavedState &s)
{
    juce::ValueTree root("CONVOLUTION_HOST");
    root.setProperty("version", 2, nullptr);
    root.setProperty("interpretation", int(s.interpretation), nullptr);
    root.addChild(s.parameters.createCopy(), -1, nullptr);
    juce::ValueTree ir("IR");
    ir.setProperty("automatic", s.routing.automatic, nullptr);
    ir.setProperty("lfeWet", s.routing.lfeWetEnabled, nullptr);
    ir.setProperty("layout", s.routing.sourceLayout, nullptr);
    ir.setProperty("path", s.request.file.getFullPathName(), nullptr);
    ir.setProperty("hash", s.hash, nullptr);
    ir.setProperty("relative", s.relativePath, nullptr);
    ir.setProperty("start", s.request.startMs, nullptr);
    ir.setProperty("end", s.request.endMs, nullptr);
    ir.setProperty("fadeIn", s.request.fadeInMs, nullptr);
    ir.setProperty("fadeOut", s.request.fadeOutMs, nullptr);
    ir.setProperty("normalise", s.request.normalise, nullptr);
    ir.setProperty("sourceHash",
                   s.request.sourceHashes.size() > 0 ? s.request.sourceHashes[0] : juce::String{}, nullptr);
    for (std::size_t i = 0; i < s.request.additionalFiles.size(); ++i)
    {
        juce::ValueTree source("SOURCE");
        source.setProperty("path", s.request.additionalFiles[i].getFullPathName(), nullptr);
        source.setProperty("hash",
                           s.request.sourceHashes.size() > int(i + 1) ? s.request.sourceHashes[int(i + 1)]
                                                                      : juce::String{},
                           nullptr);
        source.setProperty("relative",
                           s.request.additionalRelativePaths.size() > int(i)
                               ? s.request.additionalRelativePaths[int(i)]
                               : s.request.additionalFiles[i].getFileName(),
                           nullptr);
        ir.addChild(source, -1, nullptr);
    }
    for (auto r : s.routing.routes)
    {
        juce::ValueTree path("ROUTE");
        path.setProperty("input", r.input, nullptr);
        path.setProperty("output", r.output, nullptr);
        path.setProperty("irChannel", r.irChannel, nullptr);
        path.setProperty("gain", r.gain, nullptr);
        ir.addChild(path, -1, nullptr);
    }
    root.addChild(ir, -1, nullptr);
    return root;
}
bool StateStore::decode(const juce::ValueTree &root, SavedState &out)
{
    if (!root.hasType("CONVOLUTION_HOST") || (int(root["version"]) != 1 && int(root["version"]) != 2))
        return false;
    auto ir = root.getChildWithName("IR"), parameters = root.getChildWithName("PARAMETERS");
    if (!ir.isValid() || !parameters.isValid() || ir.getNumChildren() > 259)
        return false;
    SavedState s;
    s.parameters = parameters.createCopy();
    if (int(root["version"]) == 2)
    {
        const int mode = int(root["interpretation"]);
        if (mode < 0 || mode > 2)
            return false;
        s.interpretation = BusInterpretation(mode);
        if (!ir.hasProperty("automatic"))
            return false;
        s.routing.automatic = bool(ir["automatic"]);
        s.routing.lfeWetEnabled = bool(ir["lfeWet"]);
        s.routing.sourceLayout = ir["layout"].toString();
        auto id = s.routing.sourceLayout;
        bool valid = id.isEmpty() || id == "legacy-stereo";
        for (auto set : {juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo(),
                         juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create7point1(),
                         juce::AudioChannelSet::create7point1point2(),
                         juce::AudioChannelSet::create7point1point4(), juce::AudioChannelSet::ambisonic(1),
                         juce::AudioChannelSet::ambisonic(2), juce::AudioChannelSet::ambisonic(3)})
            valid = valid || id == BusLayout::fromHost(set, BusInterpretation::followHost)->identity();
        for (int n = 1; n <= 16; ++n)
            valid = valid || id == "discrete-" + juce::String(n);
        if (!valid)
            return false;
    }
    const auto path = ir["path"].toString();
    if (path.isNotEmpty() && !juce::File::isAbsolutePath(path))
        return false;
    s.request.file = path.isEmpty() ? juce::File{} : juce::File(path);
    s.hash = ir["hash"].toString();
    s.relativePath = ir["relative"].toString();
    if (juce::File::isAbsolutePath(s.relativePath) || s.relativePath.contains(".."))
        return false;
    s.request.sourceHashes.add(ir["sourceHash"].toString());
    s.request.startMs = double(ir["start"]);
    s.request.endMs = double(ir["end"]);
    s.request.fadeInMs = double(ir["fadeIn"]);
    s.request.fadeOutMs = double(ir["fadeOut"]);
    s.request.normalise = bool(ir["normalise"]);
    for (auto v : {s.request.startMs, s.request.endMs, s.request.fadeInMs, s.request.fadeOutMs})
        if (!std::isfinite(v) || v < 0 || v > 30000)
            return false;
    for (auto child : ir)
    {
        if (child.hasType("SOURCE"))
        {
            const auto sourcePath = child["path"].toString(), relative = child["relative"].toString();
            if (!juce::File::isAbsolutePath(sourcePath) || juce::File::isAbsolutePath(relative) ||
                relative.contains(".."))
                return false;
            s.request.additionalFiles.push_back(juce::File(sourcePath));
            s.request.sourceHashes.add(child["hash"].toString());
            s.request.additionalRelativePaths.add(relative);
            continue;
        }
        if (!child.hasType("ROUTE"))
            return false;
        Route r{int(child["input"]), int(child["output"]), int(child["irChannel"]), float(child["gain"])};
        if (r.input < 0 || r.input > 15 || r.output < 0 || r.output > 15 || r.irChannel < 0 ||
            r.irChannel > 15 || !std::isfinite(r.gain) || std::abs(r.gain) > 16)
            return false;
        s.routing.routes.push_back(r);
    }
    if ((!s.request.additionalFiles.empty() && s.request.additionalFiles.size() != 3) ||
        s.routing.routes.size() > 256)
        return false;
    if (int(root["version"]) == 1)
    {
        s.routing.automatic = s.routing.routes.empty();
        if (!s.routing.automatic)
            s.routing.sourceLayout = "legacy-stereo";
    }
    if (s.routing.automatic && !s.routing.routes.empty())
        return false;
    out = std::move(s);
    return true;
}
std::optional<IrRequest> StateStore::resolveRequest(const SavedState &state, const juce::StringArray &roots)
{
    IrRequest request = state.request;
    SavedState single = state;
    if (!request.additionalFiles.empty())
    {
        if (request.additionalFiles.size() != 3 || request.sourceHashes.size() != 4)
            return {};
        single.hash = request.sourceHashes[0];
    }
    request.file = resolve(single, roots);
    if (request.file == juce::File{})
        return {};
    for (std::size_t i = 0; i < request.additionalFiles.size(); ++i)
    {
        single.request.file = request.additionalFiles[i];
        single.hash = request.sourceHashes[int(i + 1)];
        single.relativePath = request.additionalRelativePaths.size() > int(i)
                                  ? request.additionalRelativePaths[int(i)]
                                  : single.request.file.getFileName();
        auto file = resolve(single, roots);
        if (file == juce::File{})
            return {};
        request.additionalFiles[i] = file;
    }
    return request;
}
juce::File StateStore::resolve(const SavedState &s, const juce::StringArray &roots)
{
    std::vector<juce::File> candidates;
    if (s.request.file != juce::File{})
        candidates.push_back(s.request.file);
    for (auto root : roots)
    {
        juce::File dir(root);
        const auto relative = s.relativePath.isNotEmpty() ? s.relativePath : s.request.file.getFileName();
        if (relative.isNotEmpty())
            candidates.push_back(dir.getChildFile(relative));
    }
    for (const auto &file : candidates)
    {
        if (!file.existsAsFile())
            continue;
        if (s.hash.isEmpty())
            return file;
        if (auto stream = file.createInputStream())
            if (juce::SHA256(*stream).toHexString() == s.hash)
                return file;
    }
    return {};
}
} // namespace ch
