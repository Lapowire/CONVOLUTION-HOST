#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
namespace ch {
class IrLibrary {
public:
 struct Root {juce::File directory;bool user=false;};
 struct Entry {juce::File file;bool user=false,favourite=false;};
 struct Snapshot {std::vector<Root> roots;std::vector<Entry> entries;juce::String error;bool scanning=false;};
 explicit IrLibrary(juce::File preferencesFile={});~IrLibrary();
 void addRoot(juce::File,bool user);void removeRoot(int);void scanAsync();void toggleFavourite(const juce::File&);
 Snapshot snapshot(const juce::String& query={},bool favouritesOnly=false) const;
 juce::StringArray rootPaths() const;
private:
 void save() const;void run();
 juce::File preferences;mutable std::mutex mutex;
 Snapshot data;std::set<juce::String> favourites;
 std::atomic<bool> stop{false};std::atomic<std::uint64_t> revision{0};std::thread worker;
};
}
