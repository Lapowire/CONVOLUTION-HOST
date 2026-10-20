#include "TestSupport.h"
#include "WavFixtures.h"
#include "state/PresetFile.h"
#include "state/StateStore.h"
TEST(preset_file_roundtrip_and_failed_save_preserves_target)
{
    test::TempDir tmp;
    ch::SavedState state;
    state.parameters = juce::ValueTree("PARAMETERS");
    juce::MemoryBlock bytes;
    juce::MemoryOutputStream out(bytes, false);
    ch::StateStore::encode(state).writeToStream(out);
    auto file = tmp.dir.getChildFile("Room.chpreset");
    REQUIRE(ch::PresetFile::save(file, bytes));
    juce::MemoryBlock loaded;
    REQUIRE(ch::PresetFile::load(file, loaded));
    REQUIRE(loaded == bytes);
    auto dir = tmp.dir.getChildFile("directory.chpreset");
    REQUIRE(dir.createDirectory());
    REQUIRE(ch::PresetFile::save(dir, bytes).failed());
    REQUIRE(dir.isDirectory());
    auto parent = tmp.dir.getChildFile("file-as-parent");
    REQUIRE(parent.replaceWithText("keep"));
    REQUIRE(ch::PresetFile::save(parent.getChildFile("preset"), bytes).failed());
    REQUIRE(parent.loadFileAsString() == "keep");
    juce::MemoryBlock invalid("bad", 3);
    REQUIRE(ch::PresetFile::save(file, invalid).failed());
    REQUIRE(ch::PresetFile::load(file, loaded));
    REQUIRE(loaded == bytes);
    REQUIRE(file.replaceWithText("corrupt"));
    REQUIRE(ch::PresetFile::load(file, loaded).failed());
    REQUIRE(loaded == bytes);
    auto large = tmp.dir.getChildFile("large");
    {
        auto stream = large.createOutputStream();
        REQUIRE(stream);
        REQUIRE(stream->setPosition(4 * 1024 * 1024));
        REQUIRE(stream->writeByte(0));
    }
    REQUIRE(ch::PresetFile::load(large, loaded).failed());
}
