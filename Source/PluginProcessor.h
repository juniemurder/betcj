#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "Shape.h"
#include "PresetManager.h"

class SheetzProcessor : public juce::AudioProcessor
{
public:
    SheetzProcessor();
    ~SheetzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static Shape defaultShape (int module);

    // shapes (message thread)
    Shape getShape (int module) const;
    void setShape (int module, Shape s);

    // presets (message thread)
    juce::var makePresetVar (const juce::String& name, const juce::String& category);
    void applyPreset (const juce::var& preset);

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    std::atomic<int>   shapeVersion { 0 };
    std::atomic<float> displayPhase { 0.f };
    juce::String presetName { "Init" };
    juce::String presetCategory { "Utility" };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float lookup (int module, double phase) const noexcept;

    Shape shapes[sheetz::numModules];
    mutable juce::CriticalSection shapeLock;
    std::array<std::atomic<float>, Shape::tableSize> tables[sheetz::numModules];

    std::atomic<float>* rateP = nullptr;
    std::atomic<float>* phaseP = nullptr;
    std::atomic<float>* smoothP = nullptr;
    std::atomic<float>* mixP = nullptr;
    std::atomic<float>* fltTypeP = nullptr;
    std::atomic<float>* fltResP = nullptr;
    std::atomic<float>* onP[sheetz::numModules] {};
    std::atomic<float>* depthP[sheetz::numModules] {};

    double sr = 44100.0;
    double ppq = 0.0;
    float vals[sheetz::numModules] { 1.f, 1.f, 1.f, 1.f, 1.f, 1.f };
    juce::dsp::StateVariableTPTFilter<float> filt[2];
    float held[2] { 0.f, 0.f };
    int holdCount[2] { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SheetzProcessor)
};
