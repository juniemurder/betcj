#include "FactoryPresets.h"
#include "Params.h"
#include "Shape.h"

namespace
{
    using S = Shape;

    struct Mod
    {
        const char* id;
        Shape shape;
        float depth = 1.f;
        float type  = 0.f;   // filter only: 0 = LP, 1 = HP
        float res   = 0.25f; // filter only
    };

    juce::var build (const juce::String& name, const juce::String& cat, const juce::String& rate,
                     float smooth, float mix, float phase, const std::vector<Mod>& mods)
    {
        auto* params = new juce::DynamicObject();
        auto* shapes = new juce::DynamicObject();

        params->setProperty ("rate",   (double) sheetz::rateLabels().indexOf (rate));
        params->setProperty ("phase",  (double) phase);
        params->setProperty ("smooth", (double) smooth);
        params->setProperty ("mix",    (double) mix);

        for (int m = 0; m < sheetz::numModules; ++m)
        {
            juce::String id (sheetz::moduleIds[m]);
            const Mod* found = nullptr;
            for (auto& md : mods) if (id == md.id) found = &md;

            params->setProperty (id + "_on", found != nullptr ? 1.0 : 0.0);
            if (found != nullptr)
            {
                params->setProperty (id + "_depth", (double) found->depth);
                shapes->setProperty (id, found->shape.toVar());
                if (id == "flt")
                {
                    params->setProperty ("flt_type", (double) found->type);
                    params->setProperty ("flt_res",  (double) found->res);
                }
            }
        }

        auto* root = new juce::DynamicObject();
        root->setProperty ("format", "SHEETZ");
        root->setProperty ("version", 1);
        root->setProperty ("name", name);
        root->setProperty ("category", cat);
        root->setProperty ("params", juce::var (params));
        root->setProperty ("shapes", juce::var (shapes));
        return juce::var (root);
    }
}

std::vector<PresetEntry> createFactoryPresets()
{
    std::vector<PresetEntry> out;
    auto add = [&] (const juce::var& v)
    {
        PresetEntry e;
        e.name = v.getProperty ("name", juce::var()).toString();
        e.category = v.getProperty ("category", juce::var()).toString();
        e.data = v;
        e.factory = true;
        out.push_back (e);
    };

    auto pump = [] (float lo, float c) { return S::make ({ { 0.f, lo, c }, { 0.75f, 1.f, 0.f }, { 1.f, 1.f, 0.f } }); };

    // ---------------- Utility ----------------
    add (build ("Init (Neutral)", "Utility", "1/4", 0.1f, 1.f, 0.f, { { "vol", S::flat (1.f) } }));

    // ---------------- Sidechain ----------------
    add (build ("Classic Pump",      "Sidechain", "1/4", 0.05f, 1.f, 0.f, { { "vol", pump (0.f, -0.35f) } }));
    add (build ("Gentle Duck",       "Sidechain", "1/4", 0.10f, 1.f, 0.f, { { "vol", pump (0.4f, -0.2f) } }));
    add (build ("Snappy Kick Duck",  "Sidechain", "1/4", 0.02f, 1.f, 0.f,
                { { "vol", S::make ({ { 0.f, 0.f, -0.7f }, { 0.45f, 1.f, 0.f }, { 1.f, 1.f, 0.f } }) } }));
    add (build ("Off-Beat Breather", "Sidechain", "1/2", 0.08f, 1.f, 0.f,
                { { "vol", S::make ({ { 0.f, 1.f, 0.f }, { 0.5f, 1.f, 0.f }, { 0.5f, 0.f, -0.45f }, { 0.9f, 1.f, 0.f }, { 1.f, 1.f, 0.f } }) } }));

    // ---------------- Gates ----------------
    add (build ("Trance Gate 16", "Gates", "1 bar", 0.03f, 1.f, 0.f,
                { { "vol", S::steps ({ 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1 }) } }));
    add (build ("Chop Suey",      "Gates", "1 bar", 0.03f, 1.f, 0.f,
                { { "vol", S::steps ({ 1, 1, 0, 1, 0, 1, 1, 0 }) } }));
    add (build ("Dropout",        "Gates", "2 bars", 0.05f, 1.f, 0.f,
                { { "vol", S::steps ({ 1, 1, 1, 1, 1, 1, 0, 0 }) } }));
    add (build ("Square Tremolo", "Gates", "1/8", 0.08f, 1.f, 0.f,
                { { "vol", S::steps ({ 1.f, 0.15f }) } }));
    add (build ("Rain Gate",      "Gates", "1 bar", 0.04f, 1.f, 0.f,
                { { "vol", S::steps ({ 1, 0.3f, 0.7f, 0.3f, 1, 0.3f, 0.5f, 0.3f, 1, 0.3f, 0.7f, 0.3f, 1, 0.5f, 0.3f, 0.7f }), 0.9f },
                  { "crs", S::steps ({ 0.f, 0.4f, 0.f, 0.6f }), 0.5f } }));

    // ---------------- Filter ----------------
    add (build ("Wobble Bass", "Filter", "1/4", 0.15f, 1.f, 0.f,
                { { "flt", S::sine (1.f, 0.12f, 0.9f), 0.95f, 0.f, 0.55f } }));
    add (build ("Slow Sweep",  "Filter", "2 bars", 0.2f, 1.f, 0.f,
                { { "flt", S::make ({ { 0.f, 0.1f, 0.4f }, { 0.5f, 0.95f, -0.4f }, { 1.f, 0.1f, 0.f } }), 1.f, 0.f, 0.35f } }));
    add (build ("HP Riser",    "Filter", "4 bars", 0.1f, 1.f, 0.f,
                { { "flt", S::make ({ { 0.f, 1.f, 0.5f }, { 1.f, 0.15f, 0.f } }), 1.f, 1.f, 0.3f } }));
    add (build ("Talk Box",    "Filter", "1/2", 0.2f, 1.f, 0.f,
                { { "flt", S::sine (2.f, 0.2f, 0.85f), 0.85f, 0.f, 0.6f } }));

    // ---------------- Pan & Width ----------------
    add (build ("Auto-Pan Smooth", "Pan & Width", "1/2", 0.1f, 1.f, 0.f,
                { { "pan", S::sine (1.f, 0.f, 1.f), 0.8f } }));
    add (build ("Ping Pong Steps", "Pan & Width", "1/4", 0.05f, 1.f, 0.f,
                { { "pan", S::steps ({ 1, 0, 1, 0 }), 1.f } }));
    add (build ("Wide Open",       "Pan & Width", "1 bar", 0.2f, 1.f, 0.f,
                { { "wid", S::sine (1.f, 0.55f, 1.f), 0.8f } }));
    add (build ("Mono Pulse",      "Pan & Width", "1/2", 0.15f, 1.f, 0.f,
                { { "wid", S::steps ({ 0.5f, 0.f }), 1.f } }));

    // ---------------- Lo-Fi & Drive ----------------
    add (build ("Crunch Pulse",   "Lo-Fi & Drive", "1/4", 0.1f, 1.f, 0.f,
                { { "drv", S::make ({ { 0.f, 1.f, -0.5f }, { 1.f, 0.1f, 0.f } }), 0.8f } }));
    add (build ("Bit Smash",      "Lo-Fi & Drive", "1/2", 0.02f, 1.f, 0.f,
                { { "crs", S::steps ({ 0.1f, 0.85f, 0.1f, 0.55f }), 1.f } }));
    add (build ("Lo-Fi Wobble",   "Lo-Fi & Drive", "1/8T", 0.2f, 1.f, 0.f,
                { { "vol", S::sine (1.f, 0.75f, 1.f), 0.6f },
                  { "drv", S::flat (0.35f), 0.5f },
                  { "crs", S::sine (1.f, 0.15f, 0.5f), 0.5f } }));
    add (build ("Warm Saturator", "Lo-Fi & Drive", "1 bar", 0.1f, 0.85f, 0.f,
                { { "drv", S::flat (0.45f), 0.6f } }));

    // ---------------- Combos ----------------
    add (build ("Sky High", "Combos", "1/4", 0.12f, 1.f, 0.f,
                { { "vol", pump (0.1f, -0.3f), 0.8f },
                  { "flt", S::sine (1.f, 0.4f, 1.f), 0.6f, 0.f, 0.3f },
                  { "wid", S::sine (1.f, 0.5f, 1.f), 0.6f } }));
    add (build ("Cloud Nine", "Combos", "2 bars", 0.4f, 1.f, 0.f,
                { { "pan", S::sine (1.f, 0.15f, 0.85f), 0.6f },
                  { "wid", S::sine (1.f, 0.5f, 1.f), 0.5f },
                  { "flt", S::make ({ { 0.f, 0.5f, 0.3f }, { 0.5f, 1.f, -0.3f }, { 1.f, 0.5f, 0.f } }), 0.5f, 0.f, 0.3f } }));
    add (build ("SHEETZ!", "Signature", "1/4", 0.1f, 1.f, 0.f,
                { { "vol", pump (0.f, -0.35f), 1.f },
                  { "flt", S::sine (1.f, 0.3f, 1.f), 0.5f, 0.f, 0.4f },
                  { "pan", S::make ({ { 0.f, 0.5f, 0.f }, { 0.25f, 0.8f, 0.f }, { 0.5f, 0.5f, 0.f }, { 0.75f, 0.2f, 0.f }, { 1.f, 0.5f, 0.f } }), 0.4f } }));

    return out;
}
