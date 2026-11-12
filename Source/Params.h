#pragma once
#include <JuceHeader.h>

namespace sheetz
{
    constexpr int numModules = 6;
    enum Module { Vol = 0, Flt, Pan, Drv, Crs, Wid };

    inline const char* const moduleIds[numModules]   = { "vol", "flt", "pan", "drv", "crs", "wid" };
    inline const char* const moduleNames[numModules] = { "Volume", "Filter", "Pan", "Drive", "Crush", "Width" };

    inline juce::StringArray rateLabels()
    {
        return { "1/32", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
    }

    // length of one shape cycle in quarter notes
    inline const double rateBeats[] = { 0.125, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 1.0, 2.0, 4.0, 8.0, 16.0 };
}
