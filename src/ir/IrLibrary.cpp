#include "IrLibrary.h"
#include <chrono>
namespace ch
{
IrLibrary::IrLibrary(juce::File p) : preferences(std::move(p))
{
    if (preferences.existsAsFile())
    {
        auto json = juce::JSON::parse(preferences);
        if (auto *object = json.getDynamicObject())
        {
            if (auto *roots = object->getProperty("roots").getArray())
                for (const auto &root : *roots)
                {
                    const auto path = root.getProperty("path", {}).toString();
                    if (juce::File::isAbsolutePath(path) && data.roots.size() < 64)
                        data.roots.push_back({juce::File(path), bool(root.getProperty("user", false))});
                }
            if (auto *list = object->getProperty("favourites").getArray())
                for (const auto &f : *list)
                    favourites.insert(f.toString());
        }
        else
            data.error = "Library preferences could not be read";
    }
    worker = std::thread([this] { run(); });
    scanAsync();
}
IrLibrary::~IrLibrary()
{
    stop.store(true);
    if (worker.joinable())
        worker.join();
}
void IrLibrary::addRoot(juce::File dir, bool user)
{
    {
        std::lock_guard lock(mutex);
        auto found = std::find_if(data.roots.begin(), data.roots.end(),
                                  [&](const Root &r) { return r.directory == dir; });
        if (found != data.roots.end())
            found->user = user;
        else if (data.roots.size() < 64)
            data.roots.push_back({dir, user});
    }
    save();
    scanAsync();
}
void IrLibrary::removeRoot(int index)
{
    {
        std::lock_guard lock(mutex);
        if (index >= 0 && std::size_t(index) < data.roots.size())
            data.roots.erase(data.roots.begin() + index);
    }
    save();
    scanAsync();
}
void IrLibrary::scanAsync()
{
    std::lock_guard lock(mutex);
    data.scanning = true;
    revision.fetch_add(1);
    uiVersion.fetch_add(1);
}
void IrLibrary::toggleFavourite(const juce::File &f)
{
    {
        std::lock_guard lock(mutex);
        const auto path = f.getFullPathName();
        if (favourites.count(path))
            favourites.erase(path);
        else
            favourites.insert(path);
        for (auto &e : data.entries)
            e.favourite = favourites.count(e.file.getFullPathName()) > 0;
    }
    uiVersion.fetch_add(1);
    save();
}
IrLibrary::Snapshot IrLibrary::snapshot(const juce::String &query, bool favouritesOnly) const
{
    std::lock_guard lock(mutex);
    auto result = data;
    result.entries.erase(std::remove_if(result.entries.begin(), result.entries.end(),
                                        [&](const Entry &e)
                                        {
                                            return (favouritesOnly && !e.favourite) ||
                                                   !e.file.getFullPathName().containsIgnoreCase(query);
                                        }),
                         result.entries.end());
    return result;
}
juce::StringArray IrLibrary::rootPaths() const
{
    std::lock_guard lock(mutex);
    juce::StringArray out;
    for (auto r : data.roots)
        out.add(r.directory.getFullPathName());
    return out;
}
void IrLibrary::save() const
{
    if (preferences == juce::File{})
        return;
    juce::Array<juce::var> roots, favs;
    {
        std::lock_guard lock(mutex);
        for (auto r : data.roots)
        {
            auto *o = new juce::DynamicObject;
            o->setProperty("path", r.directory.getFullPathName());
            o->setProperty("user", r.user);
            roots.add(juce::var(o));
        }
        for (auto f : favourites)
            favs.add(f);
    }
    auto *o = new juce::DynamicObject;
    o->setProperty("version", 1);
    o->setProperty("roots", roots);
    o->setProperty("favourites", favs);
    if (preferences.getParentDirectory().createDirectory())
        preferences.replaceWithText(juce::JSON::toString(juce::var(o)));
    else
    {
        juce::var cleanup(o);
    }
}
void IrLibrary::run()
{
    std::uint64_t completed = 0;
    while (!stop.load())
    {
        const auto ticket = revision.load();
        if (ticket == completed)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        std::vector<Root> roots;
        {
            std::lock_guard lock(mutex);
            roots = data.roots;
        }
        std::vector<Entry> entries;
        std::set<juce::String> seen;
        juce::String error;
        for (const auto &root : roots)
        {
            if (!root.directory.isDirectory() || !root.directory.hasReadAccess())
            {
                error = "A configured library directory is missing or unreadable";
                continue;
            }
            for (const auto &entry : juce::RangedDirectoryIterator(
                     root.directory, true, "*", juce::File::findFiles, juce::File::FollowSymlinks::no))
            {
                if (stop.load() || revision.load() != ticket)
                    break;
                const auto file = entry.getFile();
                if (file.hasFileExtension("wav") && file.hasReadAccess() &&
                    seen.insert(file.getFullPathName()).second)
                    entries.push_back({file, root.user, false});
                if (entries.size() >= 100000)
                {
                    error = "Library scan reached the 100000 file limit";
                    break;
                }
            }
            if (stop.load() || revision.load() != ticket || entries.size() >= 100000)
                break;
        }
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b)
                  { return a.file.getFullPathName().compareIgnoreCase(b.file.getFullPathName()) < 0; });
        {
            std::lock_guard lock(mutex);
            if (revision.load() == ticket)
            {
                for (auto &e : entries)
                    e.favourite = favourites.count(e.file.getFullPathName()) > 0;
                data.entries = std::move(entries);
                data.error = error;
                data.scanning = false;
                completed = ticket;
                uiVersion.fetch_add(1);
            }
        }
    }
}
} // namespace ch
