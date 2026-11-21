// EIDOLON — Visual DNA: preset library (categories, favorites, recents, search, save / load / rename /
// duplicate / delete / import / export) and controlled, reproducible mutation with undo / redo.
#include "Panels.h"
#include "../DNA/DNASerializer.h"
#include "../Rendering/Palette.h"

namespace eidolon::ui
{
DNAPanel::DNAPanel (EidolonCore& c) : core (c)
{
    for (auto* ed : { &nameEditor, &searchBox })
    {
        ed->setFont (Fonts::get().regular (13.0f));
        ed->setIndents (8, 6);
        content.addAndMakeVisible (ed);
    }
    nameEditor.setTooltip ("Preset name used by Save / Rename");
    searchBox.setTextToShowWhenEmpty ("Search", colours::text3);
    searchBox.onTextChange = [this] { rebuildFilter(); };
    for (int i = 2; i < kNumPresetCategories; ++i) categoryBox.addItem (kPresetCategories[i], i + 1);
    categoryBox.setTooltip ("Category the preset is saved under");
    filterBox.addItem ("All", 1);
    for (int i = 0; i < kNumPresetCategories; ++i) filterBox.addItem (kPresetCategories[i], i + 2);
    filterBox.setSelectedId (1, juce::dontSendNotification);
    filterBox.onChange = [this] { rebuildFilter(); };
    content.addAndMakeVisible (categoryBox);
    content.addAndMakeVisible (filterBox);
    content.addAndMakeVisible (presetList);
    content.addAndMakeVisible (variationGrid);
    for (auto* b : { &saveButton, &renameButton, &duplicateButton, &deleteButton, &importButton, &exportButton, &favButton,
                     &mutateButton, &variationsButton, &randomButton, &undoButton, &redoButton, &diceButton })
        content.addAndMakeVisible (b);

    saveButton.onClick = [this]
    {
        auto d = core.currentDNA();
        d.name = nameEditor.getText().trim().isEmpty() ? std::string ("Untitled") : nameEditor.getText().trim().toStdString();
        d.category = categoryBox.getText().isEmpty() ? std::string ("Custom") : categoryBox.getText().toStdString();
        juce::String err;
        if (core.library().save (d, err)) { core.dnaMeta().name = d.name; core.dnaMeta().category = d.category; core.markDirty(); setStatus ("Saved \"" + juce::String (d.name) + "\""); }
        else setStatus (err);
        rebuildFilter();
    };
    renameButton.onClick = [this]
    {
        juce::String err;
        if (auto* e = selectedEntry())
        {
            if (core.library().rename (*e, nameEditor.getText(), err)) setStatus ("Renamed"); else setStatus (err);
            rebuildFilter();
        }
        else setStatus ("Select a user preset first");
    };
    duplicateButton.onClick = [this]
    {
        juce::String err;
        if (auto* e = selectedEntry()) { if (core.library().duplicate (*e, err)) setStatus ("Duplicated"); else setStatus (err); rebuildFilter(); }
    };
    deleteButton.onClick = [this]
    {
        juce::String err;
        if (auto* e = selectedEntry()) { if (core.library().remove (*e, err)) { setStatus ("Deleted"); selectedRow = -1; } else setStatus (err); rebuildFilter(); }
    };
    favButton.onClick = [this]
    {
        const juce::String name = selectedEntry() != nullptr ? selectedEntry()->name : core.currentPresetName();
        core.library().setFavorite (name, ! core.library().isFavorite (name));
        setStatus (core.library().isFavorite (name) ? "Added to favorites" : "Removed from favorites");
        rebuildFilter();
    };
    exportButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export Visual DNA", core.library().directory().getChildFile (PresetLibrary::safeFileName (core.currentPresetName()) + ".json"), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  juce::String err;
                                  setStatus (core.library().exportTo (core.currentDNA(), f.withFileExtension ("json"), err) ? "Exported " + f.getFileName() : err);
                              });
    };
    importButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import Visual DNA", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            VisualDNA d; juce::String err;
            if (core.library().importFrom (f, d, err)) { core.loadDNA (d); setStatus ("Imported \"" + juce::String (d.name) + "\""); }
            else setStatus ("Import failed: " + err);
            rebuildFilter();
        });
    };

    // ---- mutation
    auto& ms = core.mutation;
    auto addRow = [this] (juce::Component* comp) { mutationRows.emplace_back (comp); content.addAndMakeVisible (comp); };
    addRow (new LambdaSlider ("Intensity", 0, 1, 0.001, [&ms] { return ms.intensity; }, [&ms] (float v) { ms.intensity = v; },
                              [] (float v) { return juce::String ((int) std::lround (v * 100)) + "%"; }));
    addRow (new LambdaSlider ("Seed", 1, 99999, 1, [&ms] { return (float) ms.seed; }, [&ms] (float v) { ms.seed = (uint32_t) v; }));
    addRow (new LambdaSlider ("Variation count", 1, 8, 1, [this] { return (float) variationCount; }, [this] (float v) { variationCount = (int) v; }));
    for (int g = 1; g < (int) MG::Count; ++g)
        addRow (new LambdaToggle (juce::String ("Preserve ") + mutationGroupName ((MG) g), [&ms, g] { return ms.preserve[(size_t) g]; },
                                  [&ms, g] (bool b) { ms.preserve[(size_t) g] = b; }));
    addRow (new LambdaToggle ("Preserve visual mapping", [&ms] { return ms.preserveMappings; }, [&ms] (bool b) { ms.preserveMappings = b; }));

    mutateButton.onClick = [this] { core.loadDNA (MutationEngine::mutate (core.currentDNA(), core.mutation)); setStatus ("Mutated with seed " + juce::String (core.mutation.seed)); };
    variationsButton.onClick = [this]
    {
        core.variations = MutationEngine::variations (core.currentDNA(), core.mutation, variationCount);
        setStatus (juce::String (variationCount) + " variations (click one to apply)");
        resized();
    };
    randomButton.onClick = [this] { core.loadDNA (MutationEngine::randomize (core.mutation.seed, core.currentDNA(), core.mutation.preserve)); setStatus ("Randomized"); };
    diceButton.onClick = [this] { core.mutation.seed = (uint32_t) juce::Random::getSystemRandom().nextInt ({ 1, 99999 }); refresh(); };
    undoButton.onClick = [this] { core.undo(); };
    redoButton.onClick = [this] { core.redo(); };

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    nameEditor.setText (core.currentPresetName(), false);
    rebuildFilter();
    refresh();
}

const PresetEntry* DNAPanel::selectedEntry() const
{
    if (selectedRow < 0 || selectedRow >= (int) filtered.size()) return nullptr;
    return &core.library().entries()[(size_t) filtered[(size_t) selectedRow]];
}

void DNAPanel::rebuildFilter()
{
    const auto cat = filterBox.getText();
    filtered = core.library().filtered (cat.isEmpty() ? juce::String ("All") : cat, searchBox.getText());
    if (selectedRow >= (int) filtered.size()) selectedRow = -1;
    resized();
    presetList.repaint();
}

void DNAPanel::loadEntry (int row)
{
    if (row < 0 || row >= (int) filtered.size()) return;
    const auto& e = core.library().entries()[(size_t) filtered[(size_t) row]];
    VisualDNA d; juce::String err;
    if (core.library().load (e, d, err))
    {
        core.loadDNA (d);
        core.library().markRecent (e.name);
        nameEditor.setText (e.name, false);
        const int ci = categoryBox.getNumItems() > 0 ? juce::StringArray (kPresetCategories, kNumPresetCategories).indexOf (e.category) : -1;
        if (ci >= 2) categoryBox.setSelectedId (ci + 1, juce::dontSendNotification);
        setStatus ("Loaded \"" + e.name + "\"");
    }
    else setStatus (err);
}

void DNAPanel::setStatus (const juce::String& s)
{
    content.status = s;
    content.repaint();
}

void DNAPanel::refresh()
{
    if (! nameEditor.hasKeyboardFocus (true)) nameEditor.setText (core.currentPresetName(), false);
    const int ci = juce::StringArray (kPresetCategories, kNumPresetCategories).indexOf (juce::String (core.dnaMeta().category));
    categoryBox.setSelectedId (ci >= 2 ? ci + 1 : kNumPresetCategories, juce::dontSendNotification);
    for (auto& r : mutationRows)
    {
        if (auto* s = dynamic_cast<LambdaSlider*> (r.get())) s->refresh();
        if (auto* t = dynamic_cast<LambdaToggle*> (r.get())) t->refresh();
    }
    undoButton.setEnabled (core.canUndo());
    redoButton.setEnabled (core.canRedo());
    presetList.repaint();
}

void DNAPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    const int w = std::max (220, getWidth() - 12);
    int y = 6;
    content.headings.clear();
    auto heading = [&] (const juce::String& t) { content.headings.push_back ({ { 8.0f, (float) y + 6.0f, (float) w - 8.0f, 16.0f }, t }); y += 28; };
    auto rowOf = [&] (std::initializer_list<juce::Component*> comps, int h = 26)
    {
        const int n = (int) comps.size();
        const int bw = (w - 8 - 6 * (n - 1)) / n;
        int x = 8;
        for (auto* c : comps) { c->setBounds (x, y, bw, h); x += bw + 6; }
        y += h + 6;
    };
    heading ("Current visual identity");
    nameEditor.setBounds (8, y, w - 8, 28); y += 34;
    rowOf ({ &categoryBox, &favButton });
    rowOf ({ &saveButton, &renameButton, &duplicateButton, &deleteButton });
    rowOf ({ &importButton, &exportButton });
    content.statusArea = { 8.0f, (float) y, (float) w - 8.0f, 18.0f };
    y += 22;
    heading ("Library");
    rowOf ({ &filterBox, &searchBox });
    const int listH = std::max (1, (int) filtered.size()) * 26;
    presetList.setBounds (8, y, w - 8, listH);
    y += listH + 10;
    heading ("Mutation");
    for (auto& r : mutationRows) { r->setBounds (8, y, w - 8, 26); y += 26; }
    y += 6;
    rowOf ({ &mutateButton, &variationsButton, &randomButton });
    rowOf ({ &diceButton, &undoButton, &redoButton });
    const int cells = (int) core.variations.size();
    const int gh = cells > 0 ? ((cells + 1) / 2) * 46 : 0;
    variationGrid.setBounds (8, y, w - 8, gh);
    y += gh + 12;
    content.setBounds (0, 0, w, y);
}

void DNAPanel::Content::paint (juce::Graphics& g)
{
    for (auto& h : headings) drawSectionHeading (g, h.first, h.second);
    g.setColour (colours::accent.withAlpha (0.85f));
    g.setFont (Fonts::get().regular (11.5f));
    g.drawText (status, statusArea, juce::Justification::centredLeft, true);
}

void DNAPanel::PresetList::paint (juce::Graphics& g)
{
    const auto& lib = owner.core.library();
    const auto current = owner.core.currentPresetName();
    for (int row = 0; row < (int) owner.filtered.size(); ++row)
    {
        const auto& e = lib.entries()[(size_t) owner.filtered[(size_t) row]];
        auto r = juce::Rectangle<int> (0, row * 26, getWidth(), 25);
        if (row == owner.selectedRow) { g.setColour (colours::accent.withAlpha (0.12f)); g.fillRoundedRectangle (r.toFloat(), 3.0f); }
        else if (row % 2 == 0) { g.setColour (colours::bg2.withAlpha (0.6f)); g.fillRect (r); }
        if (e.name == current) { g.setColour (colours::accent); g.fillRect (r.getX(), r.getY() + 5, 2, r.getHeight() - 10); }
        auto star = r.removeFromRight (22);
        if (lib.isFavorite (e.name)) drawIcon (g, Icon::Star, star.toFloat().withSizeKeepingCentre (14, 14), colours::warm);
        g.setFont (Fonts::get().regular (10.5f));
        g.setColour (colours::text3);
        g.drawText ((e.factory ? "FACTORY  " : "") + e.category.toUpperCase(), r.removeFromRight (120), juce::Justification::centredRight, true);
        g.setFont (Fonts::get().regular (12.5f));
        g.setColour (e.name == current ? colours::text : colours::text2);
        g.drawText (e.name, r.reduced (8, 0), juce::Justification::centredLeft, true);
    }
    if (owner.filtered.empty())
    {
        g.setColour (colours::text3);
        g.setFont (Fonts::get().regular (12.0f));
        g.drawText ("No presets in this category", getLocalBounds(), juce::Justification::centred);
    }
}

void DNAPanel::PresetList::mouseDown (const juce::MouseEvent& e)
{
    const int row = rowAt (e.y);
    if (row < 0 || row >= (int) owner.filtered.size()) return;
    owner.selectedRow = row;
    if (e.x > getWidth() - 22)
    {
        const auto& entry = owner.core.library().entries()[(size_t) owner.filtered[(size_t) row]];
        owner.core.library().setFavorite (entry.name, ! owner.core.library().isFavorite (entry.name));
    }
    else owner.loadEntry (row);
    repaint();
}

void DNAPanel::PresetList::mouseDoubleClick (const juce::MouseEvent& e) { owner.loadEntry (rowAt (e.y)); }

void DNAPanel::Variations::paint (juce::Graphics& g)
{
    const auto& vars = owner.core.variations;
    const int cw = (getWidth() - 6) / 2;
    for (int i = 0; i < (int) vars.size(); ++i)
    {
        auto r = juce::Rectangle<int> ((i % 2) * (cw + 6), (i / 2) * 46, cw, 40);
        g.setColour (colours::bg3);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (colours::line2);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 4.0f, 1.0f);
        const auto pal = makePalette (vars[(size_t) i].params);
        for (int k = 0; k < 5; ++k)
        {
            g.setColour (pal.at ((float) k / 5.0f));
            g.fillRect (r.getX() + 8 + k * 10, r.getBottom() - 12, 9, 5);
        }
        g.setColour (colours::text);
        g.setFont (Fonts::get().regular (11.5f));
        g.drawText (juce::String (vars[(size_t) i].name), r.reduced (8, 4).withTrimmedBottom (10), juce::Justification::topLeft, true);
        g.setColour (colours::text3);
        g.setFont (Fonts::get().regular (10.0f));
        const int geo = vars[(size_t) i].params.geti (P::PianoGeometry);
        g.drawText (juce::String (paramInfo (P::PianoGeometry).choiceName (geo)), r.reduced (8, 4), juce::Justification::bottomRight, true);
    }
}

void DNAPanel::Variations::mouseDown (const juce::MouseEvent& e)
{
    const int cw = (getWidth() - 6) / 2;
    const int i = (e.y / 46) * 2 + (e.x > cw ? 1 : 0);
    if (i >= 0 && i < (int) owner.core.variations.size())
    {
        owner.core.loadDNA (owner.core.variations[(size_t) i]);
        owner.setStatus ("Applied " + juce::String (owner.core.variations[(size_t) i].name));
    }
}
} // namespace eidolon::ui
