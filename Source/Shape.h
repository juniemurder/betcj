#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <vector>

// A shape is a list of points (x = time 0..1, y = value 0..1).
// Each point's "c" (-1..1) bends the segment that starts at that point.
struct ShapePoint { float x = 0.f, y = 0.f, c = 0.f; };

struct Shape
{
    static constexpr int tableSize = 1024; // power of two

    std::vector<ShapePoint> pts;

    static float curveT (float t, float c) noexcept
    {
        if (std::abs (c) < 1.0e-4f) return t;
        return std::pow (t, std::pow (8.0f, c));
    }

    float evaluate (float phase) const noexcept
    {
        if (pts.size() < 2) return pts.empty() ? 0.f : pts[0].y;
        phase = juce::jlimit (0.f, 1.f, phase);

        size_t i = 0;
        while (i + 2 < pts.size() && pts[i + 1].x <= phase) ++i;

        const auto& a = pts[i];
        const auto& b = pts[i + 1];
        const float dx = b.x - a.x;
        if (dx <= 1.0e-6f) return b.y;

        const float t = juce::jlimit (0.f, 1.f, (phase - a.x) / dx);
        return a.y + (b.y - a.y) * curveT (t, a.c);
    }

    void sanitize()
    {
        if (pts.size() < 2) { *this = flat (pts.empty() ? 1.f : pts[0].y); return; }
        for (auto& p : pts)
        {
            p.x = juce::jlimit (0.f, 1.f, p.x);
            p.y = juce::jlimit (0.f, 1.f, p.y);
            p.c = juce::jlimit (-1.f, 1.f, p.c);
        }
        std::stable_sort (pts.begin(), pts.end(), [] (const ShapePoint& a, const ShapePoint& b) { return a.x < b.x; });
        pts.front().x = 0.f;
        pts.back().x = 1.f;
    }

    void fillTable (std::atomic<float>* table) const
    {
        for (int i = 0; i < tableSize; ++i)
            table[i].store (evaluate ((float) i / (float) tableSize));
    }

    void invert() { for (auto& p : pts) p.y = 1.f - p.y; }

    void reverse()
    {
        auto old = pts;
        const int n = (int) old.size();
        for (int j = 0; j < n; ++j)
        {
            pts[(size_t) j].x = 1.f - old[(size_t) (n - 1 - j)].x;
            pts[(size_t) j].y = old[(size_t) (n - 1 - j)].y;
            pts[(size_t) j].c = (j < n - 1) ? -old[(size_t) (n - 2 - j)].c : 0.f;
        }
    }

    // ---- JSON ----
    juce::var toVar() const
    {
        juce::Array<juce::var> a;
        for (auto& p : pts)
        {
            juce::Array<juce::var> q;
            q.add (juce::var ((double) p.x));
            q.add (juce::var ((double) p.y));
            q.add (juce::var ((double) p.c));
            a.add (juce::var (q));
        }
        return juce::var (a);
    }

    static Shape fromVar (const juce::var& v, const Shape& fallback)
    {
        Shape s;
        if (auto* arr = v.getArray())
            for (auto& e : *arr)
                if (auto* q = e.getArray())
                    if (q->size() >= 2)
                    {
                        ShapePoint p;
                        p.x = (float) (double) (*q)[0];
                        p.y = (float) (double) (*q)[1];
                        p.c = q->size() > 2 ? (float) (double) (*q)[2] : 0.f;
                        s.pts.push_back (p);
                    }
        if (s.pts.size() < 2) return fallback;
        s.sanitize();
        return s;
    }

    // ---- generators ----
    static Shape make (std::initializer_list<ShapePoint> l)
    {
        Shape s; s.pts = l; s.sanitize(); return s;
    }

    static Shape flat (float y)
    {
        Shape s; s.pts.push_back ({ 0.f, y, 0.f }); s.pts.push_back ({ 1.f, y, 0.f }); return s;
    }

    static Shape steps (const std::vector<float>& v)
    {
        Shape s;
        const int n = (int) v.size();
        for (int i = 0; i < n; ++i)
        {
            s.pts.push_back ({ (float) i / (float) n,       v[(size_t) i], 0.f });
            s.pts.push_back ({ (float) (i + 1) / (float) n, v[(size_t) i], 0.f });
        }
        s.sanitize();
        return s;
    }

    static Shape sine (float cycles, float lo, float hi)
    {
        Shape s;
        const int n = juce::jmax (4, juce::roundToInt (cycles * 16.f));
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n;
            const float v = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * cycles * ph);
            s.pts.push_back ({ ph, lo + (hi - lo) * v, 0.f });
        }
        s.sanitize();
        return s;
    }

    static Shape triangle (float lo, float hi)
    {
        return make ({ { 0.f, lo, 0.f }, { 0.5f, hi, 0.f }, { 1.f, lo, 0.f } });
    }
};
