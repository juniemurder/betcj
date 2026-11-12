#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioParameterFloatAttributes pct()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.f)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.f; });
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SheetzProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Rate", sheetz::rateLabels(), 5));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "phase", 1 },  "Phase",  NormalisableRange<float> (0.f, 1.f), 0.f, pct()));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "smooth", 1 }, "Smooth", NormalisableRange<float> (0.f, 1.f), 0.1f, pct()));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 },    "Mix",    NormalisableRange<float> (0.f, 1.f), 1.f, pct()));

    for (int m = 0; m < sheetz::numModules; ++m)
    {
        String id (sheetz::moduleIds[m]);
        String nm (sheetz::moduleNames[m]);
        l.add (std::make_unique<AudioParameterBool>  (ParameterID { id + "_on", 1 }, nm + " On", m == sheetz::Vol));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id + "_depth", 1 }, nm + " Depth",
                                                     NormalisableRange<float> (0.f, 1.f), m == sheetz::Vol ? 1.f : 0.6f, pct()));
    }

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "flt_type", 1 }, "Filter Type", StringArray { "Lowpass", "Highpass" }, 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "flt_res", 1 }, "Filter Resonance", NormalisableRange<float> (0.f, 1.f), 0.25f, pct()));
    return l;
}

Shape SheetzProcessor::defaultShape (int m)
{
    switch (m)
    {
        case sheetz::Vol: return Shape::make ({ { 0.f, 0.f, -0.35f }, { 0.75f, 1.f, 0.f }, { 1.f, 1.f, 0.f } });
        case sheetz::Flt: return Shape::sine (1.f, 0.2f, 1.f);
        case sheetz::Pan: return Shape::sine (1.f, 0.f, 1.f);
        case sheetz::Drv: return Shape::make ({ { 0.f, 1.f, -0.4f }, { 1.f, 0.1f, 0.f } });
        case sheetz::Crs: return Shape::make ({ { 0.f, 0.8f, -0.4f }, { 1.f, 0.f, 0.f } });
        case sheetz::Wid: return Shape::sine (1.f, 0.25f, 0.9f);
        default:          return Shape::flat (1.f);
    }
}

SheetzProcessor::SheetzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    rateP    = apvts.getRawParameterValue ("rate");
    phaseP   = apvts.getRawParameterValue ("phase");
    smoothP  = apvts.getRawParameterValue ("smooth");
    mixP     = apvts.getRawParameterValue ("mix");
    fltTypeP = apvts.getRawParameterValue ("flt_type");
    fltResP  = apvts.getRawParameterValue ("flt_res");

    for (int m = 0; m < sheetz::numModules; ++m)
    {
        juce::String id (sheetz::moduleIds[m]);
        onP[m]    = apvts.getRawParameterValue (id + "_on");
        depthP[m] = apvts.getRawParameterValue (id + "_depth");
        setShape (m, defaultShape (m));
    }
}

void SheetzProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto& f : filt)
    {
        f.prepare (spec);
        f.reset();
        f.setCutoffFrequency (20000.f);
    }
    for (auto& v : vals) v = 1.f;
    held[0] = held[1] = 0.f;
    holdCount[0] = holdCount[1] = 0;
}

bool SheetzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

float SheetzProcessor::lookup (int m, double phase) const noexcept
{
    const double pos = phase * (double) Shape::tableSize;
    const int i0 = (int) pos;
    const float f = (float) (pos - (double) i0);
    const int a = i0 & (Shape::tableSize - 1);
    const int b = (i0 + 1) & (Shape::tableSize - 1);
    const float va = tables[m][(size_t) a].load (std::memory_order_relaxed);
    const float vb = tables[m][(size_t) b].load (std::memory_order_relaxed);
    return va + (vb - va) * f;
}

void SheetzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    // ---- transport ----
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition()) ppq = *p;   // otherwise free-run
        }
    const double inc = bpm / 60.0 / sr;

    // ---- parameters ----
    const int rateIdx   = juce::jlimit (0, 9, (int) rateP->load());
    const double beats  = sheetz::rateBeats[rateIdx];
    const double offset = (double) phaseP->load();
    const float mix     = mixP->load();
    const float sm      = smoothP->load();
    const float coef    = 1.f - std::exp (-1.f / (float) (sr * (0.0003 + (double) (sm * sm) * 0.15)));

    bool on[sheetz::numModules];
    float depth[sheetz::numModules];
    for (int m = 0; m < sheetz::numModules; ++m)
    {
        on[m] = onP[m]->load() > 0.5f;
        depth[m] = depthP[m]->load();
    }

    const bool hp = fltTypeP->load() > 0.5f;
    const float q = 0.707f + fltResP->load() * 9.f;
    for (auto& f : filt)
    {
        f.setType (hp ? juce::dsp::StateVariableTPTFilterType::highpass
                      : juce::dsp::StateVariableTPTFilterType::lowpass);
        f.setResonance (q);
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    double ph = 0.0;

    for (int i = 0; i < n; ++i)
    {
        ph = ppq / beats + offset;
        ph -= std::floor (ph);

        for (int m = 0; m < sheetz::numModules; ++m)
            if (on[m])
                vals[m] += coef * (lookup (m, ph) - vals[m]);

        const float dl = L[i], dr = R[i];
        float l = dl, r = dr;

        // FILTER
        if (on[sheetz::Flt])
        {
            const float vEff = 1.f - depth[sheetz::Flt] * (1.f - vals[sheetz::Flt]);
            if ((i & 3) == 0)
            {
                float f = 20.f * std::pow (1000.f, hp ? 1.f - vEff : vEff);
                f = juce::jmin (f, 0.45f * (float) sr);
                filt[0].setCutoffFrequency (f);
                filt[1].setCutoffFrequency (f);
            }
            l = filt[0].processSample (0, l);
            r = filt[1].processSample (0, r);
        }

        // DRIVE
        if (on[sheetz::Drv])
        {
            const float amt = depth[sheetz::Drv] * vals[sheetz::Drv];
            if (amt > 0.001f)
            {
                const float g = 1.f + amt * 24.f;
                const float comp = 1.f / std::pow (g, 0.3f);
                const float b = juce::jmin (1.f, amt * 6.f);
                l += b * (std::tanh (l * g) * comp - l);
                r += b * (std::tanh (r * g) * comp - r);
            }
        }

        // CRUSH
        if (on[sheetz::Crs])
        {
            const float amt = depth[sheetz::Crs] * vals[sheetz::Crs];
            if (amt > 0.001f)
            {
                const float bits = 16.f - amt * 13.f;
                const float lv = std::pow (2.f, bits - 1.f);
                const int factor = 1 + (int) (amt * amt * 24.f);
                float* io[2] = { &l, &r };
                for (int c = 0; c < 2; ++c)
                {
                    if (holdCount[c] <= 0)
                    {
                        held[c] = std::round (*io[c] * lv) / lv;
                        holdCount[c] = factor;
                    }
                    --holdCount[c];
                    *io[c] = held[c];
                }
            }
        }

        // WIDTH
        if (on[sheetz::Wid])
        {
            const float w = 1.f + depth[sheetz::Wid] * (2.f * vals[sheetz::Wid] - 1.f);
            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r) * w;
            l = mid + side;
            r = mid - side;
        }

        // PAN
        if (on[sheetz::Pan])
        {
            const float p = depth[sheetz::Pan] * (2.f * vals[sheetz::Pan] - 1.f);
            const float angle = (p + 1.f) * juce::MathConstants<float>::pi * 0.25f;
            l *= std::cos (angle) * juce::MathConstants<float>::sqrt2;
            r *= std::sin (angle) * juce::MathConstants<float>::sqrt2;
        }

        // VOLUME
        if (on[sheetz::Vol])
        {
            const float gain = 1.f - depth[sheetz::Vol] * (1.f - vals[sheetz::Vol]);
            l *= gain;
            r *= gain;
        }

        L[i] = dl + mix * (l - dl);
        R[i] = dr + mix * (r - dr);

        ppq += inc;
    }

    displayPhase.store ((float) ph);
}

// ---------------------------------------------------------------- shapes
Shape SheetzProcessor::getShape (int m) const
{
    const juce::ScopedLock sl (shapeLock);
    return shapes[m];
}

void SheetzProcessor::setShape (int m, Shape s)
{
    s.sanitize();
    {
        const juce::ScopedLock sl (shapeLock);
        shapes[m] = s;
    }
    s.fillTable (tables[m].data());
    ++shapeVersion;
}

// ---------------------------------------------------------------- presets
juce::var SheetzProcessor::makePresetVar (const juce::String& name, const juce::String& category)
{
    auto* params = new juce::DynamicObject();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            params->setProperty (rp->paramID, (double) rp->convertFrom0to1 (rp->getValue()));

    auto* shapesObj = new juce::DynamicObject();
    for (int m = 0; m < sheetz::numModules; ++m)
        shapesObj->setProperty (sheetz::moduleIds[m], getShape (m).toVar());

    auto* root = new juce::DynamicObject();
    root->setProperty ("format", "SHEETZ");
    root->setProperty ("version", 1);
    root->setProperty ("name", name);
    root->setProperty ("category", category);
    root->setProperty ("params", juce::var (params));
    root->setProperty ("shapes", juce::var (shapesObj));
    return juce::var (root);
}

void SheetzProcessor::applyPreset (const juce::var& v)
{
    if (! PresetManager::isValid (v)) return;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    auto paramsVar = v.getProperty ("params", juce::var());
    if (auto* obj = paramsVar.getDynamicObject())
        for (auto& nv : obj->getProperties())
            if (auto* p = apvts.getParameter (nv.name.toString()))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) (double) nv.value));

    auto shapesVar = v.getProperty ("shapes", juce::var());
    for (int m = 0; m < sheetz::numModules; ++m)
    {
        auto sv = shapesVar.getProperty (juce::Identifier (sheetz::moduleIds[m]), juce::var());
        setShape (m, Shape::fromVar (sv, defaultShape (m)));
    }

    presetName     = v.getProperty ("name", "Init").toString();
    presetCategory = v.getProperty ("category", "User").toString();
}

// ---------------------------------------------------------------- state
void SheetzProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;

    xml->setAttribute ("presetName", presetName);
    xml->setAttribute ("presetCategory", presetCategory);
    for (int m = 0; m < sheetz::numModules; ++m)
    {
        auto* e = xml->createNewChildElement ("SHAPE");
        e->setAttribute ("id", sheetz::moduleIds[m]);
        e->setAttribute ("pts", juce::JSON::toString (getShape (m).toVar(), true));
    }
    copyXmlToBinary (*xml, dest);
}

void SheetzProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    presetName     = xml->getStringAttribute ("presetName", "Init");
    presetCategory = xml->getStringAttribute ("presetCategory", "Utility");

    Shape loaded[sheetz::numModules];
    for (int m = 0; m < sheetz::numModules; ++m) loaded[m] = defaultShape (m);

    for (auto* e : xml->getChildWithTagNameIterator ("SHAPE"))
        for (int m = 0; m < sheetz::numModules; ++m)
            if (e->getStringAttribute ("id") == sheetz::moduleIds[m])
                loaded[m] = Shape::fromVar (juce::JSON::parse (e->getStringAttribute ("pts")), defaultShape (m));

    xml->deleteAllChildElementsWithTagName ("SHAPE");
    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    for (int m = 0; m < sheetz::numModules; ++m) setShape (m, loaded[m]);
}

juce::AudioProcessorEditor* SheetzProcessor::createEditor() { return new SheetzEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SheetzProcessor(); }
