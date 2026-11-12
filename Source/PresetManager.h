#pragma once
#include <JuceHeader.h>
#include <vector>

struct PresetEntry
{
    juce::String name, category;
    juce::var data;
    bool factory = false;
    juce::File file;
};

// Presets are plain JSON files with the extension ".sheetz".
// User presets live in  <Documents>/SHEETZ/Presets  (any subfolder).
class PresetManager
{
public:
    PresetManager() { rescan(); }

    static juce::File userFolder();
    static bool isValid (const juce::var& v);

    void rescan();
    const std::vector<PresetEntry>& entries() const { return list; }

    // Copies valid presets (files or whole folders) into the user library.
    // Returns the destination files of everything that was imported.
    juce::Array<juce::File> importFiles (const juce::Array<juce::File>& files);

    juce::File saveUser (const juce::String& name, const juce::String& category, const juce::var& data);
    int indexOfFile (const juce::File& f) const;
    int indexOfName (const juce::String& name) const;

private:
    std::vector<PresetEntry> list;
};
