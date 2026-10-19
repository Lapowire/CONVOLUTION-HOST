#include "TestSupport.h"
#include "WavFixtures.h"
#include "ir/IrLibrary.h"
#include <chrono>
#include <thread>
static void waitLibrary(ch::IrLibrary& lib){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(lib.snapshot().scanning && std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(2));REQUIRE(!lib.snapshot().scanning);}
TEST(library_roots_search_favourites_and_persistence) {
 test::TempDir tmp;juce::AudioBuffer<float> b(1,1);b.setSample(0,0,1);auto hall=tmp.wav("Hall.wav",b);tmp.wav("Plate.WAV",b);tmp.dir.getChildFile("ignore.txt").replaceWithText("x");
 auto prefs=tmp.dir.getChildFile("prefs.json");
 {ch::IrLibrary lib(prefs);lib.addRoot(tmp.dir,false);lib.addRoot(tmp.dir,true);waitLibrary(lib);
  REQUIRE(lib.snapshot().entries.size()==2);REQUIRE(lib.snapshot("hall").entries.size()==1);REQUIRE(lib.snapshot().roots.size()==1);
  lib.toggleFavourite(hall);REQUIRE(lib.snapshot({},true).entries.size()==1);}
 {ch::IrLibrary lib(prefs);waitLibrary(lib);REQUIRE(lib.snapshot({},true).entries.size()==1);lib.removeRoot(0);waitLibrary(lib);REQUIRE(lib.snapshot().entries.empty());}
}
TEST(library_does_not_follow_symlink_cycles_and_can_be_destroyed_scanning) {
 test::TempDir tmp;juce::AudioBuffer<float> b(1,1);b.setSample(0,0,1);tmp.wav("Hall.wav",b);
 tmp.dir.createSymbolicLink(tmp.dir.getChildFile("cycle"),true);
 {ch::IrLibrary lib;lib.addRoot(tmp.dir,true);waitLibrary(lib);REQUIRE(lib.snapshot().entries.size()==1);lib.scanAsync();}
 ch::IrLibrary missing;missing.addRoot(tmp.dir.getChildFile("absent"),false);waitLibrary(missing);REQUIRE(missing.snapshot().error.isNotEmpty());
}
