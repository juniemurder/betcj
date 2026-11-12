#include "PluginEditor.h"
#include <map>

namespace
{
    void setupKnob (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    }

    void setupLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (14.f, juce::Font::bold)));
        l.setInterceptsMouseClicks (false, false);
    }
}

SheetzEditor::SheetzEditor (SheetzProcessor& p)
    : AudioProcessorEditor (&p), proc (p), shapeEditor (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (background);
    addAndMakeVisible (shapeEditor);

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &saveBtn, &importBtn, &exportBtn })
        addAndMakeVisible (*b);

    prevBtn.onClick   = [this] { stepPreset (-1); };
    nextBtn.onClick   = [this] { stepPreset (+1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick   = [this] { savePreset(); };
    exportBtn.onClick = [this] { exportPreset(); };
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import SHEETZ presets",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.sheetz;*.json");
        juce::Component::SafePointer<SheetzEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe != nullptr && fc.getResults().size() > 0)
                                      safe->importFiles (fc.getResults());
                              });
    };

    for (int m = 0; m < sheetz::numModules; ++m)
    {
        juce::String id (sheetz::moduleIds[m]);

        addAndMakeVisible (tabs[m]);
        tabs[m].setButtonText (sheetz::moduleNames[m]);
        tabs[m].setClickingTogglesState (true);
        tabs[m].setRadioGroupId (1);
        tabs[m].onClick = [this, m] { selectModule (m); };

        addAndMakeVisible (onBtns[m]);
        onAtt[m] = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id + "_on", onBtns[m]);

        addAndMakeVisible (depth[m]);
        setupKnob (depth[m]);
        depthAtt[m] = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id + "_depth", depth[m]);
    }

    for (auto* s : { &phase, &smooth, &mix, &res })
    {
        addAndMakeVisible (*s);
        setupKnob (*s);
    }
    phaseAtt  = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "phase", phase);
    smoothAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "smooth", smooth);
    mixAtt    = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "mix", mix);
    resAtt    = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "flt_res", res);

    addAndMakeVisible (rateBox);
    rateBox.addItemList (sheetz::rateLabels(), 1);
    rateAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "rate", rateBox);

    addAndMakeVisible (fltType);
    fltType.addItemList ({ "Lowpass", "Highpass" }, 1);
    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "flt_type", fltType);

    addAndMakeVisible (snapBtn);
    snapBtn.setToggleState (true, juce::dontSendNotification);
    snapBtn.onClick = [this] { shapeEditor.setSnap (snapBtn.getToggleState()); };

    setupLabel (depthLbl, "Depth");
    setupLabel (rateLbl, "Rate");
    setupLabel (phaseLbl, "Phase");
    setupLabel (smoothLbl, "Smooth");
    setupLabel (mixLbl, "Mix");
    setupLabel (typeLbl, "Filter Type");
    setupLabel (resLbl, "Resonance");
    for (auto* l : { &depthLbl, &rateLbl, &phaseLbl, &smoothLbl, &mixLbl, &typeLbl, &resLbl })
        addAndMakeVisible (*l);

    presetBtn.setButtonText (proc.presetName);
    presetIndex = proc.presets.indexOfName (proc.presetName);

    tabs[0].setToggleState (true, juce::dontSendNotification);
    selectModule (0);

    setSize (900, 580);
}

SheetzEditor::~SheetzEditor()
{
    setLookAndFeel (nullptr);
}

void SheetzEditor::resized()
{
    auto b = getLocalBounds();
    background.setBounds (b);

    auto header = b.removeFromTop (72).reduced (22, 16);
    header.removeFromLeft (270);
    exportBtn.setBounds (header.removeFromRight (74));  header.removeFromRight (6);
    importBtn.setBounds (header.removeFromRight (74));  header.removeFromRight (6);
    saveBtn.setBounds   (header.removeFromRight (64));  header.removeFromRight (14);
    nextBtn.setBounds   (header.removeFromRight (36));  header.removeFromRight (6);
    prevBtn.setBounds   (header.removeFromLeft (36));   header.removeFromLeft (6);
    presetBtn.setBounds (header);

    auto tabRow = b.removeFromTop (46).reduced (18, 6);
    const int cw = tabRow.getWidth() / sheetz::numModules;
    for (int m = 0; m < sheetz::numModules; ++m)
    {
        auto cell = tabRow.removeFromLeft (cw).reduced (3, 0);
        onBtns[m].setBounds (cell.removeFromLeft (28));
        tabs[m].setBounds (cell);
    }

    shapeEditor.setBounds (b.removeFromTop (296).reduced (18, 4));

    auto panel = b.reduced (18, 6);
    background.setPanels ({ panel.toFloat() });

    auto ctl = panel.reduced (14, 8);
    auto col = [&] (int i) { return juce::Rectangle<int> (ctl.getX() + i * 104, ctl.getY(), 104, ctl.getHeight()); };
    auto knobArea = [&] (int i) { return col (i).withTrimmedTop (22); };
    auto labelArea = [&] (int i) { return col (i).withHeight (20); };
    auto comboArea = [&] (int i) { return col (i).withTrimmedTop (46).withHeight (28).reduced (4, 0); };

    for (auto& d : depth) d.setBounds (knobArea (0));
    depthLbl.setBounds (labelArea (0));

    rateLbl.setBounds (labelArea (1));   rateBox.setBounds (comboArea (1));
    phaseLbl.setBounds (labelArea (2));  phase.setBounds (knobArea (2));
    smoothLbl.setBounds (labelArea (3)); smooth.setBounds (knobArea (3));
    mixLbl.setBounds (labelArea (4));    mix.setBounds (knobArea (4));
    typeLbl.setBounds (labelArea (5));   fltType.setBounds (comboArea (5));
    resLbl.setBounds (labelArea (6));    res.setBounds (knobArea (6));
    snapBtn.setBounds (col (7).withTrimmedTop (46).withHeight (28));
}

void SheetzEditor::selectModule (int m)
{
    selected = m;
    shapeEditor.setModule (m);
    for (int k = 0; k < sheetz::numModules; ++k)
        depth[k].setVisible (k == m);

    const bool f = (m == sheetz::Flt);
    fltType.setVisible (f);
    typeLbl.setVisible (f);
    res.setVisible (f);
    resLbl.setVisible (f);
}

// ------------------------------------------------------------------ presets
void SheetzEditor::loadEntry (int idx)
{
    auto& list = proc.presets.entries();
    if (idx < 0 || idx >= (int) list.size()) return;

    proc.applyPreset (list[(size_t) idx].data);
    presetIndex = idx;
    presetBtn.setButtonText (list[(size_t) idx].name);
    shapeEditor.setModule (selected);
}

void SheetzEditor::stepPreset (int dir)
{
    const int n = (int) proc.presets.entries().size();
    if (n == 0) return;
    loadEntry (presetIndex < 0 ? 0 : (presetIndex + dir + n) % n);
}

void SheetzEditor::showPresetMenu()
{
    auto& list = proc.presets.entries();
    std::map<juce::String, juce::PopupMenu> cats;
    for (size_t i = 0; i < list.size(); ++i)
    {
        auto key = list[i].factory ? list[i].category : "[User] " + list[i].category;
        cats[key].addItem ((int) i + 1, list[i].name, true, (int) i == presetIndex);
    }

    juce::PopupMenu menu;
    for (auto& [name, sub] : cats)
        if (! name.startsWith ("[User]")) menu.addSubMenu (name, sub);

    bool sep = false;
    for (auto& [name, sub] : cats)
        if (name.startsWith ("[User]"))
        {
            if (! sep) { menu.addSeparator(); sep = true; }
            menu.addSubMenu (name, sub);
        }

    menu.addSeparator();
    menu.addItem (100000, "Open presets folder");
    menu.addItem (100001, "Rescan presets");

    juce::Component::SafePointer<SheetzEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 100000)
        {
            PresetManager::userFolder().createDirectory();
            PresetManager::userFolder().revealToUser();
        }
        else if (r == 100001)
        {
            safe->proc.presets.rescan();
        }
        else
            safe->loadEntry (r - 1);
    });
}

void SheetzEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Give your preset a name and category.",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presetName == "Init" ? juce::String ("My Preset") : proc.presetName, "Name");
    w->addTextEditor ("cat", "User", "Category");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<SheetzEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;

        auto name = w->getTextEditorContents ("name").trim();
        auto cat  = w->getTextEditorContents ("cat").trim();
        if (name.isEmpty()) return;
        if (cat.isEmpty()) cat = "User";

        auto data = safe->proc.makePresetVar (name, cat);
        auto file = safe->proc.presets.saveUser (name, cat, data);
        if (file != juce::File())
            safe->loadEntry (safe->proc.presets.indexOfFile (file));
    }), true);
}

void SheetzEditor::importFiles (const juce::Array<juce::File>& files)
{
    auto added = proc.presets.importFiles (files);
    if (added.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import",
                                                "No valid SHEETZ presets (.sheetz) were found.");
        return;
    }
    loadEntry (proc.presets.indexOfFile (added.getFirst()));
}

void SheetzEditor::exportPreset()
{
    chooser = std::make_unique<juce::FileChooser> ("Export preset",
                  juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile (juce::File::createLegalFileName (proc.presetName) + ".sheetz"),
                  "*.sheetz");
    juce::Component::SafePointer<SheetzEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (safe == nullptr || f == juce::File()) return;
                              f = f.withFileExtension ("sheetz");
                              auto v = safe->proc.makePresetVar (safe->proc.presetName, safe->proc.presetCategory);
                              f.replaceWithText (juce::JSON::toString (v));
                          });
}

bool SheetzEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.isDirectory() || file.hasFileExtension ("sheetz") || file.hasFileExtension ("json"))
            return true;
    }
    return false;
}

void SheetzEditor::filesDropped (const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> fs;
    for (auto& f : files) fs.add (juce::File (f));
    importFiles (fs);
}
