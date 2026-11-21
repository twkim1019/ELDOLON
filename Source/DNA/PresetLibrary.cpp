#include "PresetLibrary.h"
#include "DNASerializer.h"
#include "FactoryPresets.h"

namespace eidolon
{
static juce::File defaultDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("EIDOLON").getChildFile ("Presets");
}

PresetLibrary::PresetLibrary() : PresetLibrary (defaultDirectory()) {}

PresetLibrary::PresetLibrary (const juce::File& d) : dir (d)
{
    loadMeta();
    refresh();
}

juce::String PresetLibrary::safeFileName (const juce::String& name)
{
    auto s = juce::File::createLegalFileName (name.trim()).substring (0, 64);
    return s.isEmpty() ? juce::String ("Untitled") : s;
}

void PresetLibrary::loadMeta()
{
    favorites.clear(); recents.clear();
    const auto meta = dir.getChildFile ("library.json");
    if (! meta.existsAsFile()) return;
    const auto v = juce::JSON::parse (meta.loadFileAsString());
    if (auto* f = v.getProperty ("favorites", {}).getArray()) for (auto& x : *f) favorites.addIfNotAlreadyThere (x.toString());
    if (auto* r = v.getProperty ("recent", {}).getArray()) for (auto& x : *r) recents.add (x.toString());
}

void PresetLibrary::saveMeta() const
{
    if (! dir.createDirectory()) return;
    auto* o = new juce::DynamicObject();
    juce::Array<juce::var> f, r;
    for (auto& s : favorites) f.add (s);
    for (auto& s : recents) r.add (s);
    o->setProperty ("favorites", f);
    o->setProperty ("recent", r);
    dir.getChildFile ("library.json").replaceWithText (juce::JSON::toString (juce::var (o), false));
}

void PresetLibrary::refresh()
{
    all.clear();
    const auto& fp = factoryPresets();
    for (int i = 0; i < (int) fp.size(); ++i)
        all.push_back ({ juce::String (fp[(size_t) i].name), juce::String (fp[(size_t) i].category), true, i, {} });
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
        files.sort();
        for (auto& f : files)
        {
            if (f.getFileName() == "library.json") continue;
            const auto v = juce::JSON::parse (f.loadFileAsString());
            if (v.getProperty ("format", "").toString() != "EIDOLON-VisualDNA") continue;
            all.push_back ({ v.getProperty ("name", f.getFileNameWithoutExtension()).toString(),
                             v.getProperty ("category", "Custom").toString(), false, -1, f });
        }
    }
}

std::vector<int> PresetLibrary::filtered (const juce::String& category, const juce::String& search) const
{
    std::vector<int> out;
    auto match = [&] (const PresetEntry& e) { return search.isEmpty() || e.name.containsIgnoreCase (search) || e.category.containsIgnoreCase (search); };
    if (category == "Recent")
    {
        for (auto& r : recents) { const int i = indexOf (r); if (i >= 0 && match (all[(size_t) i])) out.push_back (i); }
        return out;
    }
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& e = all[(size_t) i];
        const bool ok = category.isEmpty() || category == "All" || (category == "Favorites" ? isFavorite (e.name) : e.category == category);
        if (ok && match (e)) out.push_back (i);
    }
    return out;
}

int PresetLibrary::indexOf (const juce::String& name) const
{
    for (int i = (int) all.size(); --i >= 0;) if (all[(size_t) i].name == name) return i;   // user presets shadow factory ones
    return -1;
}

bool PresetLibrary::load (const PresetEntry& e, VisualDNA& out, juce::String& error) const
{
    if (e.factory)
    {
        const auto& fp = factoryPresets();
        if (e.factoryIndex < 0 || e.factoryIndex >= (int) fp.size()) { error = "Unknown factory preset"; return false; }
        out = fp[(size_t) e.factoryIndex];
        return true;
    }
    if (! e.file.existsAsFile()) { error = "Preset file is missing"; return false; }
    return dnaFromJson (e.file.loadFileAsString(), out, error);
}

bool PresetLibrary::save (const VisualDNA& d, juce::String& error, bool overwrite)
{
    if (! dir.createDirectory()) { error = "Cannot create preset folder " + dir.getFullPathName(); return false; }
    auto f = dir.getChildFile (safeFileName (d.name) + ".json");
    if (f.existsAsFile() && ! overwrite) f = f.getNonexistentSibling();
    if (! f.replaceWithText (dnaToJson (d))) { error = "Cannot write " + f.getFullPathName(); return false; }
    refresh();
    return true;
}

bool PresetLibrary::rename (const PresetEntry& e, const juce::String& newName, juce::String& error)
{
    if (e.factory) { error = "Factory presets are read-only (duplicate it first)"; return false; }
    VisualDNA d;
    if (! load (e, d, error)) return false;
    const auto oldName = e.name;
    const auto oldFile = e.file;
    d.name = newName.trim().substring (0, 80).toStdString();
    if (d.name.empty()) { error = "Name cannot be empty"; return false; }
    if (! save (d, error, false)) return false;
    oldFile.deleteFile();
    if (favorites.contains (oldName)) { favorites.removeString (oldName); favorites.add (newName); }
    recents.removeString (oldName);
    saveMeta();
    refresh();
    return true;
}

bool PresetLibrary::duplicate (const PresetEntry& e, juce::String& error)
{
    VisualDNA d;
    if (! load (e, d, error)) return false;
    d.name = (e.name + " copy").toStdString();
    if (e.factory) d.category = "Custom";
    return save (d, error, false);
}

bool PresetLibrary::remove (const PresetEntry& e, juce::String& error)
{
    if (e.factory) { error = "Factory presets cannot be deleted"; return false; }
    if (! e.file.deleteFile()) { error = "Cannot delete " + e.file.getFullPathName(); return false; }
    favorites.removeString (e.name);
    recents.removeString (e.name);
    saveMeta();
    refresh();
    return true;
}

bool PresetLibrary::exportTo (const VisualDNA& d, const juce::File& file, juce::String& error) const
{
    if (! file.replaceWithText (dnaToJson (d))) { error = "Cannot write " + file.getFullPathName(); return false; }
    return true;
}

bool PresetLibrary::importFrom (const juce::File& file, VisualDNA& out, juce::String& error)
{
    if (! file.existsAsFile() || file.getSize() > 4 * 1024 * 1024) { error = "File missing or too large"; return false; }
    if (! dnaFromJson (file.loadFileAsString(), out, error)) return false;
    return save (out, error, false);
}

void PresetLibrary::setFavorite (const juce::String& name, bool fav)
{
    if (fav) favorites.addIfNotAlreadyThere (name); else favorites.removeString (name);
    saveMeta();
}

void PresetLibrary::markRecent (const juce::String& name)
{
    recents.removeString (name);
    recents.insert (0, name);
    while (recents.size() > 12) recents.remove (recents.size() - 1);
    saveMeta();
}
} // namespace eidolon
