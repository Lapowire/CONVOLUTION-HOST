#include "PresetFile.h"
#include "StateStore.h"
namespace ch
{
namespace
{
bool valid(const juce::MemoryBlock &bytes)
{
    if (bytes.isEmpty() || bytes.getSize() > 4 * 1024 * 1024)
        return false;
    SavedState state;
    return StateStore::decode(juce::ValueTree::readFromData(bytes.getData(), bytes.getSize()), state);
}
} // namespace
juce::Result PresetFile::save(const juce::File &target, const juce::MemoryBlock &bytes)
{
    if (!valid(bytes))
        return juce::Result::fail("Could not save preset: invalid state");
    if (target.isDirectory() || !target.getParentDirectory().createDirectory())
        return juce::Result::fail("Could not save preset: invalid destination");
    juce::TemporaryFile temporary(target);
    {
        juce::FileOutputStream stream(temporary.getFile());
        if (!stream.openedOk() || !stream.write(bytes.getData(), bytes.getSize()))
            return juce::Result::fail("Could not save preset: write failed");
        stream.flush();
        if (stream.getStatus().failed())
            return juce::Result::fail("Could not save preset: flush failed");
    }
    return temporary.overwriteTargetFileWithTemporary()
               ? juce::Result::ok()
               : juce::Result::fail("Could not save preset: replace failed");
}
juce::Result PresetFile::load(const juce::File &file, juce::MemoryBlock &destination)
{
    juce::MemoryBlock bytes;
    if (!file.existsAsFile() || file.getSize() > 4 * 1024 * 1024 || !file.loadFileAsData(bytes) ||
        !valid(bytes))
        return juce::Result::fail("Could not load preset: missing, invalid or larger than 4 MiB");
    destination = std::move(bytes);
    return juce::Result::ok();
}
} // namespace ch
