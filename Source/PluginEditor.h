#pragma once
#include "PluginProcessor.h"
#include "UI.h"
#include "ShapeEditor.h"

class SheetzEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget
{
public:
    explicit SheetzEditor (SheetzProcessor&);
    ~SheetzEditor() override;

    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void selectModule (int m);
    void showPresetMenu();
    void loadEntry (int index);
    void stepPreset (int dir);
    void savePreset();
    void importFiles (const juce::Array<juce::File>& files);
    void exportPreset();

    SheetzProcessor& proc;
    SkyLookAndFeel laf;
    SkyBackground background;
    ShapeEditor shapeEditor;

    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" },
                     saveBtn { "Save" }, importBtn { "Import" }, exportBtn { "Export" };
    juce::TextButton tabs[sheetz::numModules];
    juce::ToggleButton onBtns[sheetz::numModules];
    juce::Slider depth[sheetz::numModules];
    juce::Slider phase, smooth, mix, res;
    juce::ComboBox rateBox, fltType;
    juce::ToggleButton snapBtn { "Snap" };
    juce::Label depthLbl, rateLbl, phaseLbl, smoothLbl, mixLbl, typeLbl, resLbl;

    std::unique_ptr<APVTS::ButtonAttachment> onAtt[sheetz::numModules];
    std::unique_ptr<APVTS::SliderAttachment> depthAtt[sheetz::numModules];
    std::unique_ptr<APVTS::SliderAttachment> phaseAtt, smoothAtt, mixAtt, resAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> rateAtt, typeAtt;

    std::unique_ptr<juce::FileChooser> chooser;
    int presetIndex = -1;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SheetzEditor)
};
