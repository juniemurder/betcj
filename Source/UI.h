#pragma once
#include <JuceHeader.h>

namespace col
{
    inline const juce::Colour skyDeep (0xff3a9be0);
    inline const juce::Colour ink     (0xff2b6597);
    inline const juce::Colour skyTop  (0xff7cc6f2);
    inline const juce::Colour skyLow  (0xffe3f4ff);
}

inline void drawCloud (juce::Graphics& g, float cx, float cy, float s, float alpha)
{
    struct Blob { float dx, dy, r; };
    static const Blob blobs[] = { { -1.2f, 0.2f, 0.55f }, { -0.6f, -0.2f, 0.75f }, { 0.2f, -0.35f, 0.9f },
                                  { 1.0f, -0.05f, 0.7f }, { 1.6f, 0.25f, 0.5f },  { 0.2f, 0.25f, 0.8f } };

    for (int pass = 0; pass < 2; ++pass)             // soft halo, then body
    {
        const float grow = pass == 0 ? 1.25f : 1.f;
        g.setColour (juce::Colours::white.withAlpha (pass == 0 ? alpha * 0.35f : alpha));
        for (auto& b : blobs)
        {
            const float r = b.r * s * grow;
            g.fillEllipse (cx + b.dx * s - r, cy + b.dy * s - r, 2.f * r, 2.f * r);
        }
    }
}

class SkyBackground : public juce::Component
{
public:
    SkyBackground()
    {
        setInterceptsMouseClicks (false, false);
        setBufferedToImage (true);
    }

    void setPanels (std::vector<juce::Rectangle<float>> p) { panels = std::move (p); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (col::skyTop, 0.f, 0.f, col::skyLow, 0.f, b.getBottom(), false));
        g.fillRect (b);

        juce::Random r (42);
        for (int i = 0; i < 14; ++i)
        {
            const float cx = r.nextFloat() * b.getWidth();
            const float cy = r.nextFloat() * b.getHeight();
            const float s  = 18.f + r.nextFloat() * 34.f;
            drawCloud (g, cx, cy, s, 0.35f + r.nextFloat() * 0.45f);
        }

        for (auto& p : panels)
        {
            g.setColour (juce::Colours::white.withAlpha (0.42f));
            g.fillRoundedRectangle (p, 20.f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (p.reduced (0.5f), 20.f, 1.5f);
        }

        auto title = juce::Rectangle<int> (22, 8, 280, 58);
        g.setFont (juce::Font (juce::FontOptions (46.f, juce::Font::bold)));
        g.setColour (col::skyDeep.withAlpha (0.45f));
        g.drawText ("SHEETZ!", title.translated (2, 3), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.drawText ("SHEETZ!", title, juce::Justification::centredLeft);
    }

private:
    std::vector<juce::Rectangle<float>> panels;
};

class SkyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkyLookAndFeel()
    {
        using namespace juce;
        setColour (Slider::textBoxTextColourId, col::ink);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (TextButton::textColourOffId, col::ink);
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (ToggleButton::textColourId, col::ink);
        setColour (ComboBox::backgroundColourId, Colours::white.withAlpha (0.85f));
        setColour (ComboBox::textColourId, col::ink);
        setColour (ComboBox::outlineColourId, Colours::white);
        setColour (ComboBox::arrowColourId, col::skyDeep);
        setColour (Label::textColourId, col::ink);
        setColour (PopupMenu::backgroundColourId, Colour (0xfff2faff));
        setColour (PopupMenu::textColourId, col::ink);
        setColour (PopupMenu::highlightedBackgroundColourId, col::skyDeep);
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (AlertWindow::backgroundColourId, Colour (0xfff2faff));
        setColour (AlertWindow::textColourId, col::ink);
        setColour (TextEditor::backgroundColourId, Colours::white);
        setColour (TextEditor::textColourId, col::ink);
        setColour (TextEditor::outlineColourId, col::skyDeep.withAlpha (0.4f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float d = juce::jmin (area.getWidth(), area.getHeight()) - 8.f;
        auto knob = juce::Rectangle<float> (d, d).withCentre (area.getCentre());
        auto c = knob.getCentre();

        g.setColour (col::skyDeep.withAlpha (0.18f));
        g.fillEllipse (knob.translated (0.f, 3.f));
        g.setColour (juce::Colours::white);
        g.fillEllipse (knob);

        const float rad = d * 0.5f - 5.f;
        juce::Path track;
        track.addCentredArc (c.x, c.y, rad, rad, 0.f, start, end, true);
        g.setColour (col::skyDeep.withAlpha (0.2f));
        g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float angle = start + pos * (end - start);
        juce::Path val;
        val.addCentredArc (c.x, c.y, rad, rad, 0.f, start, angle, true);
        g.setColour (col::skyDeep);
        g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (col::ink);
        g.drawLine (c.x + std::sin (angle) * rad * 0.3f, c.y - std::cos (angle) * rad * 0.3f,
                    c.x + std::sin (angle) * rad * 0.72f, c.y - std::cos (angle) * rad * 0.72f, 2.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        auto fill = on ? col::skyDeep : juce::Colours::white.withAlpha (hover ? 1.f : 0.8f);
        if (down) fill = fill.darker (0.1f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? juce::Colours::white : col::skyDeep.withAlpha (0.5f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions (14.f, juce::Font::bold));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool hasText = b.getButtonText().isNotEmpty();
        const float d = 18.f;
        auto dot = juce::Rectangle<float> (d, d).withCentre (hasText ? juce::Point<float> (r.getX() + d, r.getCentreY())
                                                                     : r.getCentre());
        g.setColour (juce::Colours::white.withAlpha (hover ? 1.f : 0.85f));
        g.fillEllipse (dot);
        g.setColour (col::skyDeep);
        g.drawEllipse (dot, 2.f);
        if (b.getToggleState())
            g.fillEllipse (dot.reduced (4.5f));

        if (hasText)
        {
            g.setColour (col::ink);
            g.setFont (juce::Font (juce::FontOptions (14.f, juce::Font::bold)));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (d * 2.f + 4.f), juce::Justification::centredLeft);
        }
    }
};
