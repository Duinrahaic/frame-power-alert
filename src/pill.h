// Draws the alert pill into RGBA buffers: an icon in a countdown ring on the left, label and a battery-capacity bar
// to its right. The countdown ring is drawn as separate pie-timer layers so it can animate
// by moving overlays instead of re-uploading a texture (OpenVR overlays flash when their texture is replaced).
#pragma once
#include "icons.h"
#include "stb_truetype.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

constexpr int kTexW = 760, kTexH = 200;
constexpr float kCircleX = 100, kCircleY = 100;               // ring center in pill pixels
constexpr float kRingTexScale = 2;                            // ring layers at twice the pill's pixel density
constexpr int kRingTex = 176 * 2;                             // ring layer texture size, centered on the ring

struct Rgb { float r, g, b; };

struct Point { float x, y; };

struct Segment { Point a, b; };

// An icon as a list of SVG path strings in Lucide's 24×24 space.
struct Icon { const char* const* paths; size_t count; };

template <size_t N>
constexpr Icon iconOf(const char* const (&paths)[N]) { return {paths, N}; }

struct Style { Rgb accent; const char* label; Icon icon; };

enum class Alert { NotCharging, LowBattery, Charging };

inline Style styleFor(Alert a) {
    switch (a) {
        case Alert::NotCharging: return {{239, 68, 68}, "Not charging", iconOf(icons::kBatteryFull)};
        case Alert::LowBattery:  return {{245, 158, 11}, "Low battery", iconOf(icons::kBatteryLow)};
        case Alert::Charging:    return {{34, 197, 94}, "Charging", iconOf(icons::kBatteryCharging)};
    }
    return {};
}

// Flattens an SVG path (M L H V A Z, absolute and relative; circular arcs only) into line segments.
inline std::vector<Segment> flattenPath(const char* d) {
    std::vector<Segment> out;
    Point cur{0, 0}, start{0, 0};
    char cmd = 0;
    const char* p = d;
    auto skip = [&] { while (*p == ' ' || *p == ',') p++; };
    auto number = [&] { skip(); char* end; float v = std::strtof(p, &end); p = end; return v; };
    auto lineTo = [&](Point to) { out.push_back({cur, to}); cur = to; };

    for (skip(); *p; skip()) {
        if (std::isalpha(static_cast<unsigned char>(*p))) cmd = *p++;
        bool rel = std::islower(static_cast<unsigned char>(cmd));
        Point base = rel ? cur : Point{0, 0};
        switch (std::tolower(cmd)) {
            case 'm': {
                float x = number(), y = number();
                cur = start = {base.x + x, base.y + y};
                cmd = rel ? 'l' : 'L';  // further pairs are implicit line-tos
                break;
            }
            case 'l': { float x = number(), y = number(); lineTo({base.x + x, base.y + y}); break; }
            case 'h': lineTo({(rel ? cur.x : 0) + number(), cur.y}); break;
            case 'v': lineTo({cur.x, (rel ? cur.y : 0) + number()}); break;
            case 'z': lineTo(start); break;
            case 'a': {
                float r = number();
                number(), number();  // ry and rotation: Lucide's arcs are circular and unrotated
                bool large = number() != 0, sweep = number() != 0;
                float x = number(), y = number();
                Point to{base.x + x, base.y + y};
                // SVG endpoint → center parameterization (SVG 1.1 F.6.5), specialised to a circle.
                float hx = (cur.x - to.x) / 2, hy = (cur.y - to.y) / 2, h2 = hx * hx + hy * hy;
                r = std::max(r, std::sqrt(h2));
                float coef = std::sqrt(std::max(0.0f, (r * r - h2) / h2));
                if (large == sweep) coef = -coef;
                Point c{coef * hy + (cur.x + to.x) / 2, -coef * hx + (cur.y + to.y) / 2};
                float a0 = std::atan2(cur.y - c.y, cur.x - c.x), a1 = std::atan2(to.y - c.y, to.x - c.x);
                float delta = a1 - a0;
                if (sweep && delta < 0) delta += 2 * static_cast<float>(M_PI);
                if (!sweep && delta > 0) delta -= 2 * static_cast<float>(M_PI);
                constexpr int kSteps = 8;
                for (int i = 1; i < kSteps; i++) {
                    float a = a0 + delta * i / kSteps;
                    lineTo({c.x + r * std::cos(a), c.y + r * std::sin(a)});
                }
                lineTo(to);
                break;
            }
            default:
                std::fprintf(stderr, "unsupported path command '%c' in %s\n", cmd, d);
                std::exit(1);
        }
    }
    return out;
}

// Signed distance from (px, py) to a rounded rectangle; negative inside.
inline float roundedRectDistance(float px, float py, float x0, float y0, float x1, float y1, float radius) {
    float qx = std::fabs(px - (x0 + x1) / 2) - ((x1 - x0) / 2 - radius);
    float qy = std::fabs(py - (y0 + y1) / 2) - ((y1 - y0) / 2 - radius);
    float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f));
    return outside + std::min(std::max(qx, qy), 0.0f) - radius;
}

inline float segmentDistance(Point p, Segment s) {
    float dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    float len2 = dx * dx + dy * dy;
    float t = len2 > 0 ? std::clamp(((p.x - s.a.x) * dx + (p.y - s.a.y) * dy) / len2, 0.0f, 1.0f) : 0;
    return std::hypot(p.x - (s.a.x + t * dx), p.y - (s.a.y + t * dy));
}

class PillRenderer {
public:
    explicit PillRenderer(const char* fontPath) : base_(kTexW * kTexH * 4) {
        std::ifstream f(fontPath, std::ios::binary);
        font_data_.assign(std::istreambuf_iterator<char>(f), {});
        if (font_data_.empty() || !stbtt_InitFont(&font_, font_data_.data(), 0)) {
            std::fprintf(stderr, "cannot load font %s\n", fontPath);
            std::exit(1);
        }
        int ascent, descent, gap;
        stbtt_GetFontVMetrics(&font_, &ascent, &descent, &gap);
        ascentPlusDescent_ = ascent + descent;
    }

    // Everything but the countdown ring.
    const std::vector<uint8_t>& content(const Style& style, int batteryPercent) {
        setTarget(base_, kTexW, kTexH);
        std::fill(base_.begin(), base_.end(), 0);
        const Rgb white{255, 255, 255};

        // Opaque, so the ring mask (which repaints this background) is indistinguishable from it.
        fillRoundedRect(kInset, kInset, kTexW - kInset, kTexH - kInset, (kTexH - 2 * kInset) / 2.0f, kPillBg, 1);
        drawArc(kCircleX, kCircleY, 1, kTrack, kTrackAlpha, 0, kTwoPi, false, false);
        fillRoundedRect(kCircleX - kDiscR, kCircleY - kDiscR, kCircleX + kDiscR, kCircleY + kDiscR, kDiscR, style.accent, 0.16f);
        drawIcon(style.icon, style.accent);

        const float labelScale = stbtt_ScaleForPixelHeight(&font_, kLabelPx);
        const float labelY = (kInset + kBarY) / 2;
        float labelW = measure(style.label, labelScale);
        drawText(style.label, kTextX, labelY, labelScale, white, 1);
        drawText(std::to_string(batteryPercent) + "%", kTextX + labelW + kWordGap, labelY, labelScale, white, 0.55f);

        fillRoundedRect(kTextX, kBarY, kBarX1, kBarY + kBarH, kBarH / 2, white, 0.12f);
        float fill = std::clamp(batteryPercent / 100.0f, 0.0f, 1.0f);
        if (fill > 0) {
            float end = std::max(kTextX + (kBarX1 - kTextX) * fill, kTextX + kBarH);
            fillRoundedRect(kTextX, kBarY, end, kBarY + kBarH, kBarH / 2, style.accent, 1);
        }
        return base_;
    }

    // Pie-timer layers for an arc running clockwise from 12 o'clock to remaining × 360°. Angles are clockwise from
    // 12 o'clock. Spinner: the right half, rounded at 6 o'clock, rotated by (remaining × 360° − 180°).
    std::vector<uint8_t> ringSpinner(Rgb accent) { return ringLayer([&] { drawRingArc(accent, 0, kPi, false, true); }); }

    // Filler: the right half, rounded at 12 o'clock; shown while at least half remains.
    std::vector<uint8_t> ringFiller(Rgb accent) { return ringLayer([&] { drawRingArc(accent, 0, kPi, true, false); }); }

    // Mask: repaints the left half's background and track over the spinner, plus the arc's rounded start; shown
    // while less than half remains.
    std::vector<uint8_t> ringMask(Rgb accent) {
        return ringLayer([&] {
            const float c = kRingTex / 2.0f;
            drawArc(c, c, kRingTexScale, kPillBg, 1, kPi, kTwoPi, false, false, 3);
            drawArc(c, c, kRingTexScale, kTrack, kTrackAlpha, kPi, kTwoPi, false, false);
            drawArc(c, c, kRingTexScale, accent, 1, 0, 0, true, false);
        });
    }

    // The pill and ring flattened into one image, for previews.
    std::vector<uint8_t> composite(const Style& style, int batteryPercent, float remaining) {
        std::vector<uint8_t> pixels = content(style, batteryPercent);
        setTarget(pixels, kTexW, kTexH);
        drawArc(kCircleX, kCircleY, 1, style.accent, 1, 0, std::clamp(remaining, 0.0f, 1.0f) * kTwoPi, true, true);
        return pixels;
    }

private:
    static constexpr float kPi = static_cast<float>(M_PI), kTwoPi = 2 * kPi;
    static constexpr Rgb kPillBg{18, 18, 22}, kTrack{255, 255, 255};
    static constexpr float kTrackAlpha = 0.12f;
    static constexpr float kInset = 4, kRingR = 76, kRingW = 8, kDiscR = 64;
    static constexpr float kIconScale = 3.4f, kIconStroke = 2;
    static constexpr float kTextX = 200, kLabelPx = 56, kWordGap = 20;
    static constexpr float kBarX1 = 680, kBarY = 144, kBarH = 10;

    void setTarget(std::vector<uint8_t>& buffer, int w, int h) { target_ = &buffer; targetW_ = w; targetH_ = h; }

    void blend(int x, int y, Rgb c, float a) {
        if (x < 0 || y < 0 || x >= targetW_ || y >= targetH_ || a <= 0) return;
        uint8_t* p = &(*target_)[(y * targetW_ + x) * 4];
        float dstA = p[3] / 255.0f;
        float outA = a + dstA * (1 - a);
        auto mix = [&](float src, uint8_t dst) {
            return static_cast<uint8_t>(std::lround((src * a + dst * dstA * (1 - a)) / outA));
        };
        p[0] = mix(c.r, p[0]);
        p[1] = mix(c.g, p[1]);
        p[2] = mix(c.b, p[2]);
        p[3] = static_cast<uint8_t>(std::lround(outA * 255));
    }

    void fillRoundedRect(float x0, float y0, float x1, float y1, float radius, Rgb c, float alpha) {
        for (int y = static_cast<int>(y0) - 1; y <= static_cast<int>(y1) + 1; y++)
            for (int x = static_cast<int>(x0) - 1; x <= static_cast<int>(x1) + 1; x++) {
                float d = roundedRectDistance(x + 0.5f, y + 0.5f, x0, y0, x1, y1, radius);
                blend(x, y, c, std::clamp(0.5f - d, 0.0f, 1.0f) * alpha);
            }
    }

    void drawIcon(Icon icon, Rgb c) {
        auto map = [](Point p) { return Point{kCircleX + (p.x - 12) * kIconScale, kCircleY + (p.y - 12) * kIconScale}; };
        std::vector<Segment> segments;
        for (size_t i = 0; i < icon.count; i++)
            for (Segment s : flattenPath(icon.paths[i])) segments.push_back({map(s.a), map(s.b)});

        const float half = kIconStroke * kIconScale / 2;
        const int extent = static_cast<int>(12 * kIconScale + half + 2);
        for (int y = static_cast<int>(kCircleY) - extent; y <= static_cast<int>(kCircleY) + extent; y++)
            for (int x = static_cast<int>(kCircleX) - extent; x <= static_cast<int>(kCircleX) + extent; x++) {
                float d = 1e9f;
                for (const Segment& s : segments) d = std::min(d, segmentDistance({x + 0.5f, y + 0.5f}, s));
                blend(x, y, c, std::clamp(half - d + 0.5f, 0.0f, 1.0f));
            }
    }

    template <typename Draw>
    std::vector<uint8_t> ringLayer(Draw draw) {
        std::vector<uint8_t> pixels(kRingTex * kRingTex * 4);
        setTarget(pixels, kRingTex, kRingTex);
        draw();
        return pixels;
    }

    void drawRingArc(Rgb c, float a0, float a1, bool roundStart, bool roundEnd) {
        drawArc(kRingTex / 2.0f, kRingTex / 2.0f, kRingTexScale, c, 1, a0, a1, roundStart, roundEnd);
    }

    // The ring band between angles a0..a1 (clockwise from 12 o'clock), scaled by `scale` pixels per pill pixel.
    // Ends are cut radially unless rounded; widen grows the band on both sides.
    void drawArc(float cx, float cy, float scale, Rgb c, float alpha, float a0, float a1, bool roundStart,
                 bool roundEnd, float widen = 0) {
        const float r = kRingR * scale, half = (kRingW / 2 + widen) * scale;
        auto at = [&](float angle) { return Point{cx + r * std::sin(angle), cy - r * std::cos(angle)}; };
        const Point start = at(a0), end = at(a1);
        const int extent = static_cast<int>(r + half + 2);
        for (int y = static_cast<int>(cy) - extent; y <= static_cast<int>(cy) + extent; y++)
            for (int x = static_cast<int>(cx) - extent; x <= static_cast<int>(cx) + extent; x++) {
                float px = x + 0.5f - cx, py = y + 0.5f - cy;
                float angle = std::atan2(px, -py);
                if (angle < 0) angle += kTwoPi;
                float d = 1e9f;
                if (angle >= a0 && angle <= a1) d = std::fabs(std::hypot(px, py) - r) - half;
                if (roundStart) d = std::min(d, std::hypot(x + 0.5f - start.x, y + 0.5f - start.y) - half);
                if (roundEnd) d = std::min(d, std::hypot(x + 0.5f - end.x, y + 0.5f - end.y) - half);
                blend(x, y, c, std::clamp(0.5f - d, 0.0f, 1.0f) * alpha);
            }
    }

    float measure(const std::string& text, float scale) {
        float w = 0;
        for (size_t i = 0; i < text.size(); i++) {
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font_, text[i], &advance, &lsb);
            w += advance * scale;
            if (i + 1 < text.size()) w += stbtt_GetCodepointKernAdvance(&font_, text[i], text[i + 1]) * scale;
        }
        return w;
    }

    // Draws text vertically centered on centerY.
    void drawText(const std::string& text, float x, float centerY, float scale, Rgb c, float alpha) {
        const int baseline = static_cast<int>(std::lround(centerY + ascentPlusDescent_ * scale / 2));
        for (size_t i = 0; i < text.size(); i++) {
            int x0, y0, x1, y1;
            stbtt_GetCodepointBitmapBox(&font_, text[i], scale, scale, &x0, &y0, &x1, &y1);
            int w = x1 - x0, h = y1 - y0;
            if (w > 0 && h > 0) {
                std::vector<uint8_t> glyph(w * h);
                stbtt_MakeCodepointBitmap(&font_, glyph.data(), w, h, w, scale, scale, text[i]);
                int ox = static_cast<int>(std::lround(x)) + x0, oy = baseline + y0;
                for (int gy = 0; gy < h; gy++)
                    for (int gx = 0; gx < w; gx++) blend(ox + gx, oy + gy, c, glyph[gy * w + gx] / 255.0f * alpha);
            }
            int advance, lsb;
            stbtt_GetCodepointHMetrics(&font_, text[i], &advance, &lsb);
            x += advance * scale;
            if (i + 1 < text.size()) x += stbtt_GetCodepointKernAdvance(&font_, text[i], text[i + 1]) * scale;
        }
    }

    std::vector<uint8_t> font_data_;
    stbtt_fontinfo font_{};
    int ascentPlusDescent_ = 0;
    std::vector<uint8_t> base_;
    std::vector<uint8_t>* target_ = &base_;
    int targetW_ = kTexW, targetH_ = kTexH;
};
