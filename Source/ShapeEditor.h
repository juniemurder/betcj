#pragma once
#include "PluginProcessor.h"
#include "UI.h"
#include <optional>

// The big cloud-curve editor.
//  - double-click empty space ........ add a point
//  - drag a point ..................... move it (hold Alt to bypass snapping)
//  - drag the little hollow circle .... bend the segment (double-click to reset)
//  - right-click a point / double-click a point ... delete it
//  - right-click empty space .......... shape menu (waves, steps, random, invert, reverse)
class ShapeEditor : public juce::Component, private juce::Timer
{
public:
    explicit ShapeEditor (SheetzProcessor& p) : proc (p)
    {
        shape = proc.getShape (0);
        seenVersion = proc.shapeVersion.load();
        startTimerHz (30);
    }

    ~ShapeEditor() override { stopTimer(); }

    void setModule (int m)
    {
        module = m;
        shape = proc.getShape (m);
        seenVersion = proc.shapeVersion.load();
        repaint();
    }

    void setSnap (bool s) { snap = s; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa9dcf8), 0.f, 0.f,
                                                 juce::Colour (0xffe9f7ff), 0.f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 18.f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (b.reduced (1.f), 18.f, 2.f);

        auto a = plot();

        // big faint module name
        g.setFont (juce::Font (juce::FontOptions (64.f, juce::Font::bold)));
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawText (juce::String (sheetz::moduleNames[module]).toUpperCase(), a, juce::Justification::centred);

        // grid
        for (int i = 0; i <= 16; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (i % 4 == 0 ? 0.6f : 0.25f));
            g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 16.f), a.getY(), a.getBottom());
        }
        for (int j = 0; j <= 8; ++j)
        {
            g.setColour (juce::Colours::white.withAlpha (j % 4 == 0 ? 0.6f : 0.25f));
            g.drawHorizontalLine ((int) (a.getBottom() - a.getHeight() * (float) j / 8.f), a.getX(), a.getRight());
        }

        // curve
        juce::Path line;
        line.startNewSubPath (toScreen (shape.pts[0].x, shape.pts[0].y));
        for (size_t i = 0; i + 1 < shape.pts.size(); ++i)
        {
            auto& p0 = shape.pts[i];
            auto& p1 = shape.pts[i + 1];
            if (std::abs (p0.c) < 1.0e-4f || p1.x - p0.x < 1.0e-4f)
                line.lineTo (toScreen (p1.x, p1.y));
            else
                for (int s = 1; s <= 24; ++s)
                {
                    const float t = (float) s / 24.f;
                    line.lineTo (toScreen (p0.x + (p1.x - p0.x) * t,
                                           p0.y + (p1.y - p0.y) * Shape::curveT (t, p0.c)));
                }
        }

        juce::Path fill (line);
        fill.lineTo (a.getRight(), a.getBottom());
        fill.lineTo (a.getX(), a.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.85f), 0.f, a.getY(),
                                                 juce::Colours::white.withAlpha (0.25f), 0.f, a.getBottom(), false));
        g.fillPath (fill);

        g.setColour (col::skyDeep);
        g.strokePath (line, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // bend handles
        for (size_t i = 0; i + 1 < shape.pts.size(); ++i)
            if (auto h = handlePos ((int) i); h.has_value())
            {
                g.setColour (col::skyDeep.withAlpha (0.9f));
                g.drawEllipse (h->x - 4.5f, h->y - 4.5f, 9.f, 9.f, 1.6f);
            }

        // points
        for (auto& p : shape.pts)
        {
            auto s = toScreen (p.x, p.y);
            g.setColour (juce::Colours::white);
            g.fillEllipse (s.x - 7.f, s.y - 7.f, 14.f, 14.f);
            g.setColour (col::skyDeep);
            g.drawEllipse (s.x - 7.f, s.y - 7.f, 14.f, 14.f, 2.5f);
        }

        // playhead
        const float ph = proc.displayPhase.load();
        const float px = a.getX() + ph * a.getWidth();
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawLine (px, a.getY(), px, a.getBottom(), 2.f);
        auto dot = toScreen (ph, shape.evaluate (ph));
        g.setColour (col::ink);
        g.fillEllipse (dot.x - 4.f, dot.y - 4.f, 8.f, 8.f);

        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        g.setColour (col::ink.withAlpha (0.7f));
        g.drawText ("double-click: add point  |  drag circle: bend  |  right-click: delete / shape menu  |  Alt: no snap",
                    getLocalBounds().removeFromBottom (18).reduced (14, 0), juce::Justification::centredLeft);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragPoint = dragSeg = -1;
        const int pi = hitPoint (e.position);

        if (e.mods.isPopupMenu())
        {
            if (pi >= 0) deletePoint (pi);
            else showShapeMenu();
            return;
        }

        if (pi >= 0) { dragPoint = pi; return; }
        dragSeg = hitHandle (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const bool snapIt = snap && ! e.mods.isAltDown();

        if (dragPoint >= 0 && dragPoint < (int) shape.pts.size())
        {
            auto n = toNorm (e.position, snapIt);
            auto& p = shape.pts[(size_t) dragPoint];
            p.y = n.y;
            if (dragPoint > 0 && dragPoint < (int) shape.pts.size() - 1)
                p.x = juce::jlimit (shape.pts[(size_t) dragPoint - 1].x, shape.pts[(size_t) dragPoint + 1].x, n.x);
            commit();
        }
        else if (dragSeg >= 0 && dragSeg + 1 < (int) shape.pts.size())
        {
            auto& a = shape.pts[(size_t) dragSeg];
            auto& b = shape.pts[(size_t) dragSeg + 1];
            const float dy = b.y - a.y;
            if (std::abs (dy) > 0.01f)
            {
                const float my = toNorm (e.position, false).y;
                const float r = juce::jlimit (0.03f, 0.97f, (my - a.y) / dy);
                const float k = std::log (r) / std::log (0.5f);
                a.c = juce::jlimit (-1.f, 1.f, std::log (k) / std::log (8.f));
                commit();
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragPoint = dragSeg = -1; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;

        const int pi = hitPoint (e.position);
        if (pi >= 0) { dragPoint = -1; deletePoint (pi); return; }

        const int si = hitHandle (e.position);
        if (si >= 0) { shape.pts[(size_t) si].c = 0.f; commit(); return; }

        auto n = toNorm (e.position, snap && ! e.mods.isAltDown());
        size_t idx = 0;
        while (idx < shape.pts.size() && shape.pts[idx].x <= n.x) ++idx;
        idx = juce::jlimit ((size_t) 1, shape.pts.size() - 1, idx);
        shape.pts.insert (shape.pts.begin() + (long) idx, ShapePoint { n.x, n.y, 0.f });
        commit();
    }

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (18.f, 20.f); }

    juce::Point<float> toScreen (float x, float y) const
    {
        auto a = plot();
        return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
    }

    juce::Point<float> toNorm (juce::Point<float> p, bool snapIt) const
    {
        auto a = plot();
        float x = juce::jlimit (0.f, 1.f, (p.x - a.getX()) / a.getWidth());
        float y = juce::jlimit (0.f, 1.f, 1.f - (p.y - a.getY()) / a.getHeight());
        if (snapIt) { x = std::round (x * 16.f) / 16.f; y = std::round (y * 8.f) / 8.f; }
        return { x, y };
    }

    std::optional<juce::Point<float>> handlePos (int seg) const
    {
        auto& a = shape.pts[(size_t) seg];
        auto& b = shape.pts[(size_t) seg + 1];
        if (b.x - a.x < 0.005f || std::abs (b.y - a.y) < 0.01f) return std::nullopt;
        const float y = a.y + (b.y - a.y) * Shape::curveT (0.5f, a.c);
        return toScreen ((a.x + b.x) * 0.5f, y);
    }

    int hitPoint (juce::Point<float> p) const
    {
        int best = -1; float bd = 12.f;
        for (size_t i = 0; i < shape.pts.size(); ++i)
        {
            const float d = toScreen (shape.pts[i].x, shape.pts[i].y).getDistanceFrom (p);
            if (d < bd) { bd = d; best = (int) i; }
        }
        return best;
    }

    int hitHandle (juce::Point<float> p) const
    {
        for (int i = 0; i + 1 < (int) shape.pts.size(); ++i)
            if (auto h = handlePos (i); h.has_value() && h->getDistanceFrom (p) < 9.f)
                return i;
        return -1;
    }

    void deletePoint (int i)
    {
        if (i > 0 && i < (int) shape.pts.size() - 1)
        {
            shape.pts.erase (shape.pts.begin() + i);
            commit();
        }
    }

    void commit()
    {
        proc.setShape (module, shape);
        repaint();
    }

    void showShapeMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Shapes");
        m.addItem (1, "Reset to default");
        m.addItem (2, "Ramp down (saw)");
        m.addItem (3, "Ramp up");
        m.addItem (4, "Triangle");
        m.addItem (5, "Sine");
        m.addItem (6, "Square");
        m.addItem (7, "Staircase down (4)");
        m.addItem (8, "Staircase down (8)");
        m.addItem (9, "Random steps (8)");
        m.addItem (10, "Random steps (16)");
        m.addSeparator();
        m.addItem (20, "Invert");
        m.addItem (21, "Reverse");

        juce::Component::SafePointer<ShapeEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& s = safe->shape;
            auto randomSteps = [] (int n)
            {
                juce::Random rnd;
                std::vector<float> v;
                for (int i = 0; i < n; ++i) v.push_back ((float) rnd.nextInt (5) * 0.25f);
                return Shape::steps (v);
            };
            auto stairs = [] (int n)
            {
                std::vector<float> v;
                for (int i = 0; i < n; ++i) v.push_back (1.f - (float) i / (float) n);
                return Shape::steps (v);
            };

            switch (r)
            {
                case 1:  s = SheetzProcessor::defaultShape (safe->module); break;
                case 2:  s = Shape::make ({ { 0.f, 1.f, 0.f }, { 1.f, 0.f, 0.f } }); break;
                case 3:  s = Shape::make ({ { 0.f, 0.f, 0.f }, { 1.f, 1.f, 0.f } }); break;
                case 4:  s = Shape::triangle (0.f, 1.f); break;
                case 5:  s = Shape::sine (1.f, 0.f, 1.f); break;
                case 6:  s = Shape::steps ({ 1.f, 0.f }); break;
                case 7:  s = stairs (4); break;
                case 8:  s = stairs (8); break;
                case 9:  s = randomSteps (8); break;
                case 10: s = randomSteps (16); break;
                case 20: s.invert(); break;
                case 21: s.reverse(); break;
                default: return;
            }
            safe->commit();
        });
    }

    void timerCallback() override
    {
        const int v = proc.shapeVersion.load();
        if (v != seenVersion && dragPoint < 0 && dragSeg < 0)
        {
            shape = proc.getShape (module);
            seenVersion = v;
        }
        repaint();
    }

    SheetzProcessor& proc;
    Shape shape;
    int module = 0, dragPoint = -1, dragSeg = -1, seenVersion = -1;
    bool snap = true;
};
