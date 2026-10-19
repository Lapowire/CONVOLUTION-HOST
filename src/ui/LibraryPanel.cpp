#include "LibraryPanel.h"
namespace ch
{
LibraryPanel::LibraryPanel(IrLibrary &lib) : library(lib)
{
    for (auto *c :
         std::initializer_list<juce::Component *>{&search, &roots, &addLibrary, &addUser, &remove, &rescan,
                                                  &favourite, &onlyFavourites, &list, &info})
        addAndMakeVisible(c);
    search.setTextToShowWhenEmpty("Search your spaces...", ui::muted);
    search.onTextChange = [this] { refresh(); };
    search.setTitle("Search impulse responses");
    addLibrary.onClick = [this] { addDirectory(false); };
    addUser.onClick = [this] { addDirectory(true); };
    remove.onClick = [this] { library.removeRoot(roots.getSelectedId() - 1); };
    rescan.onClick = [this] { library.scanAsync(); };
    onlyFavourites.setClickingTogglesState(true);
    onlyFavourites.onClick = [this] { refresh(); };
    favourite.onClick = [this]
    {
        int row = list.getSelectedRow();
        if (row >= 0 && std::size_t(row) < data.entries.size())
            library.toggleFavourite(data.entries[std::size_t(row)].file);
    };
    list.setRowHeight(48);
    list.setOutlineThickness(0);
    info.setFont(ui::font(11));
    info.setColour(juce::Label::textColourId, ui::muted);
    refresh();
    startTimerHz(4);
}
LibraryPanel::~LibraryPanel()
{
    stopTimer();
    list.setModel(nullptr);
}
void LibraryPanel::refresh()
{
    version = library.version();
    data = library.snapshot(search.getText(), onlyFavourites.getToggleState());
    int selection = roots.getSelectedId();
    roots.clear(juce::dontSendNotification);
    for (std::size_t i = 0; i < data.roots.size(); ++i)
        roots.addItem((data.roots[i].user ? "USER / " : "LIB / ") + data.roots[i].directory.getFileName(),
                      int(i) + 1);
    if (!data.roots.empty())
        roots.setSelectedId(juce::jlimit(1, int(data.roots.size()), std::max(1, selection)),
                            juce::dontSendNotification);
    list.updateContent();
    list.repaint();
    info.setText(data.scanning             ? "Scanning library..."
                 : data.error.isNotEmpty() ? data.error
                                           : juce::String(data.entries.size()) + " impulse responses",
                 juce::dontSendNotification);
}
void LibraryPanel::addDirectory(bool user)
{
    if (choosing)
        return;
    choosing = true;
    chooser = std::make_unique<juce::FileChooser>(user ? "Add user IR directory" : "Add library directory",
                                                  juce::File{}, "*", true);
    auto safe = juce::Component::SafePointer<LibraryPanel>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
                         [safe, user](const juce::FileChooser &fc)
                         {
                             if (!safe)
                                 return;
                             safe->choosing = false;
                             auto dir = fc.getResult();
                             if (dir.isDirectory())
                                 safe->library.addRoot(dir, user);
                         });
}
void LibraryPanel::paint(juce::Graphics &g)
{
    ui::card(g, getLocalBounds().toFloat());
    g.setColour(ui::muted);
    g.setFont(ui::font(11, true));
    g.drawText("YOUR IMPULSE LIBRARY", 16, 12, getWidth() - 32, 20, juce::Justification::left);
}
void LibraryPanel::resized()
{
    int w = getWidth() - 32;
    search.setBounds(16, 42, w, 30);
    roots.setBounds(16, 82, w, 28);
    addLibrary.setBounds(16, 120, w / 2 - 4, 28);
    addUser.setBounds(20 + w / 2, 120, w / 2 - 4, 28);
    onlyFavourites.setBounds(16, 158, w, 28);
    list.setBounds(8, 198, getWidth() - 16, std::max(40, getHeight() - 302));
    favourite.setBounds(16, getHeight() - 96, w, 26);
    remove.setBounds(16, getHeight() - 60, w / 2 - 4, 25);
    rescan.setBounds(20 + w / 2, getHeight() - 60, w / 2 - 4, 25);
    info.setBounds(12, getHeight() - 32, getWidth() - 24, 26);
}
void LibraryPanel::paintListBoxItem(int row, juce::Graphics &g, int w, int h, bool selected)
{
    if (row < 0 || std::size_t(row) >= data.entries.size())
        return;
    const auto &entry = data.entries[std::size_t(row)];
    if (selected)
    {
        g.setColour(ui::accent.withAlpha(0.10f));
        g.fillRoundedRectangle(juce::Rectangle<float>(4, 1, float(w - 8), float(h - 2)), 5);
        g.setColour(ui::accent);
        g.fillRect(4, 8, 2, h - 16);
    }
    g.setColour(entry.favourite ? ui::amber : ui::muted);
    g.setFont(ui::font(12));
    g.drawText(entry.favourite ? "*" : "/", 12, 9, 14, 20, juce::Justification::left);
    g.setColour(ui::text);
    g.setFont(ui::font(13, true));
    g.drawText(entry.file.getFileNameWithoutExtension(), 30, 5, w - 40, 22, juce::Justification::left);
    g.setColour(ui::muted);
    g.setFont(ui::font(10));
    g.drawText((entry.user ? "USER / " : "LIB / ") + entry.file.getParentDirectory().getFileName(), 30, 26,
               w - 40, 16, juce::Justification::left);
}
void LibraryPanel::listBoxItemClicked(int row, const juce::MouseEvent &)
{
    activateRow(row);
}
void LibraryPanel::activateRow(int row)
{
    if (row >= 0 && std::size_t(row) < data.entries.size() && onSelect)
        onSelect(data.entries[std::size_t(row)].file);
}
} // namespace ch
