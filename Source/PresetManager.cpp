#include "PresetManager.h"
#include "FactoryPresets.h"

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("SHEETZ").getChildFile ("Presets");
}

bool PresetManager::isValid (const juce::var& v)
{
    return v.isObject() && v.getProperty ("format", juce::var()).toString() == "SHEETZ";
}

void PresetManager::rescan()
{
    list = createFactoryPresets();

    std::vector<PresetEntry> user;
    auto folder = userFolder();
    if (folder.isDirectory())
    {
        for (auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.sheetz"))
        {
            auto v = juce::JSON::parse (f);
            if (! isValid (v)) continue;

            PresetEntry e;
            e.name     = v.getProperty ("name", f.getFileNameWithoutExtension()).toString();
            e.category = v.getProperty ("category", "User").toString();
            e.data     = v;
            e.file     = f;
            user.push_back (e);
        }
    }

    std::sort (user.begin(), user.end(), [] (const PresetEntry& a, const PresetEntry& b)
    {
        auto c = a.category.compareIgnoreCase (b.category);
        return c != 0 ? c < 0 : a.name.compareIgnoreCase (b.name) < 0;
    });

    list.insert (list.end(), user.begin(), user.end());
}

juce::Array<juce::File> PresetManager::importFiles (const juce::Array<juce::File>& files)
{
    juce::Array<juce::File> toRead, imported;

    for (auto& f : files)
    {
        if (f.isDirectory())
            toRead.addArray (f.findChildFiles (juce::File::findFiles, true, "*.sheetz;*.json"));
        else
            toRead.add (f);
    }

    auto dir = userFolder().getChildFile ("Imported");
    dir.createDirectory();

    for (auto& f : toRead)
    {
        auto v = juce::JSON::parse (f);
        if (! isValid (v)) continue;

        auto name = v.getProperty ("name", f.getFileNameWithoutExtension()).toString();
        auto dest = dir.getChildFile (juce::File::createLegalFileName (name) + ".sheetz").getNonexistentSibling();
        if (dest.replaceWithText (juce::JSON::toString (v)))
            imported.add (dest);
    }

    rescan();
    return imported;
}

juce::File PresetManager::saveUser (const juce::String& name, const juce::String& category, const juce::var& data)
{
    juce::ignoreUnused (category);
    auto dir = userFolder().getChildFile ("User");
    dir.createDirectory();
    auto dest = dir.getChildFile (juce::File::createLegalFileName (name) + ".sheetz");
    if (! dest.replaceWithText (juce::JSON::toString (data))) return {};
    rescan();
    return dest;
}

int PresetManager::indexOfFile (const juce::File& f) const
{
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].file == f) return (int) i;
    return -1;
}

int PresetManager::indexOfName (const juce::String& name) const
{
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name == name) return (int) i;
    return -1;
}
