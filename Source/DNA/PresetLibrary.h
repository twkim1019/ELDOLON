#pragma once
// EIDOLON — Visual DNA preset library: factory presets (built in) + user presets stored as JSON in
// %APPDATA%\EIDOLON\Presets (Windows) / ~/.config/EIDOLON/Presets (Linux), favorites and recents.
#include "VisualDNA.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace eidolon
{
struct PresetEntry
{
    juce::String name, category;
    bool factory = false;
    int factoryIndex = -1;
    juce::File file;
};

class PresetLibrary
{
public:
    PresetLibrary();
    explicit PresetLibrary (const juce::File& directory);

    juce::File directory() const { return dir; }
    void refresh();
    const std::vector<PresetEntry>& entries() const { return all; }
    std::vector<int> filtered (const juce::String& category, const juce::String& search = {}) const;   // "All", "Favorites", "Recent", ...

    bool load (const PresetEntry& e, VisualDNA& out, juce::String& error) const;
    bool save (const VisualDNA& d, juce::String& error, bool overwrite = true);
    bool rename (const PresetEntry& e, const juce::String& newName, juce::String& error);
    bool duplicate (const PresetEntry& e, juce::String& error);
    bool remove (const PresetEntry& e, juce::String& error);
    bool exportTo (const VisualDNA& d, const juce::File& file, juce::String& error) const;
    bool importFrom (const juce::File& file, VisualDNA& out, juce::String& error);

    bool isFavorite (const juce::String& name) const { return favorites.contains (name); }
    void setFavorite (const juce::String& name, bool fav);
    void markRecent (const juce::String& name);
    const juce::StringArray& recent() const { return recents; }
    int indexOf (const juce::String& name) const;

    static juce::String safeFileName (const juce::String& name);

private:
    void loadMeta();
    void saveMeta() const;

    juce::File dir;
    std::vector<PresetEntry> all;
    juce::StringArray favorites, recents;
};
} // namespace eidolon
