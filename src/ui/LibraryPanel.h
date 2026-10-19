#pragma once
#include "Theme.h"
#include "ir/IrLibrary.h"
namespace ch
{
class LibraryPanel : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
  public:
    explicit LibraryPanel(IrLibrary &);
    ~LibraryPanel() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    std::function<void(const juce::File &)> onSelect;

  private:
    int getNumRows() override { return int(data.entries.size()); }
    void paintListBoxItem(int, juce::Graphics &, int, int, bool) override;
    void listBoxItemClicked(int, const juce::MouseEvent &) override;
    void returnKeyPressed(int row) override { activateRow(row); }
    void activateRow(int);
    void timerCallback() override
    {
        if (version != library.version())
            refresh();
    }
    void refresh();
    void addDirectory(bool);
    IrLibrary &library;
    IrLibrary::Snapshot data;
    std::uint64_t version = 0;
    juce::TextEditor search;
    juce::ComboBox roots;
    juce::TextButton addLibrary{"Library +"}, addUser{"User +"}, remove{"Remove"}, rescan{"Rescan"},
        favourite{"Favourite"}, onlyFavourites{"Favourites"};
    juce::ListBox list{"IR Library", this};
    juce::Label info;
    std::unique_ptr<juce::FileChooser> chooser;
    bool choosing = false;
};
} // namespace ch
