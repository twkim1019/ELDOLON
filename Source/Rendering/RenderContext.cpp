#include "RenderContext.h"
#include "../UI/Theme.h"
#include <algorithm>

namespace eidolon
{
RenderContext::RenderContext (juce::Graphics& gr, const Camera& c, const juce::Image& gi, const std::vector<juce::Image>* sp)
    : g (gr), cam (c), glowImage (gi), area (c.viewport()), sprites (sp)
{
    projScratch.reserve (512);
    vertScratch.reserve (512);
    faceScratch.reserve (512);
}

juce::Image RenderContext::createGlowImage (int size)
{
    juce::Image img (juce::Image::SingleChannel, size, size, true);
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    const float c = (float) (size - 1) * 0.5f;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const float d = std::sqrt ((x - c) * (x - c) + (y - c) * (y - c)) / c;
            float v = d >= 1.0f ? 0.0f : std::exp (-d * d * 4.2f) * (1.0f - d * d);
            v = v * 0.85f + (d < 0.18f ? (0.18f - d) / 0.18f * 0.15f : 0.0f);
            *bd.getPixelPointer (x, y) = (juce::uint8) juce::jlimit (0, 255, (int) (v * 255.0f));
        }
    return img;
}

std::vector<juce::Image> RenderContext::createGlowSprites()
{
    std::vector<juce::Image> v;
    for (int r = 0; r <= kMaxSprite; ++r)
    {
        const int size = std::max (1, r * 2);
        juce::Image img (juce::Image::SingleChannel, size, size, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float c = (float) size * 0.5f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const float d = std::sqrt (((float) x + 0.5f - c) * ((float) x + 0.5f - c) + ((float) y + 0.5f - c) * ((float) y + 0.5f - c)) / std::max (0.5f, c);
                float val = d >= 1.0f ? 0.0f : std::exp (-d * d * 4.2f) * (1.0f - d * d);
                val = val * 0.85f + (d < 0.18f ? (0.18f - d) / 0.18f * 0.15f : 0.0f);
                *bd.getPixelPointer (x, y) = (juce::uint8) juce::jlimit (0, 255, (int) (val * 255.0f));
            }
        v.push_back (img);
    }
    return v;
}

void RenderContext::glow (float x, float y, float radius, juce::Colour c, float a)
{
    a *= alpha;
    if (a <= 0.004f || radius < 0.5f) return;
    if (x + radius < area.getX() || x - radius > area.getRight() || y + radius < area.getY() || y - radius > area.getBottom()) return;
    a = juce::jlimit (0.0f, 1.0f, a);
    radius = std::min (radius, std::max (area.getWidth(), area.getHeight()) * 0.75f);
    if (radius > (float) kMaxSprite)
    {
        // large halos: a radial gradient is far cheaper than resampling the sprite (matters on the CPU path)
        juce::ColourGradient grad (c.withMultipliedAlpha (a), x, y, c.withAlpha (0.0f), x + radius, y, true);
        grad.addColour (0.18, c.withMultipliedAlpha (a * 0.62f));
        grad.addColour (0.42, c.withMultipliedAlpha (a * 0.24f));
        grad.addColour (0.7, c.withMultipliedAlpha (a * 0.06f));
        g.setGradientFill (grad);
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        return;
    }
    g.setColour (c.withMultipliedAlpha (a));
    if (sprites != nullptr && (int) sprites->size() > kMaxSprite)
    {
        const int r = juce::jlimit (1, kMaxSprite, (int) std::lround (radius));
        g.drawImageAt ((*sprites)[(size_t) r], (int) std::lround (x) - r, (int) std::lround (y) - r, true);
        return;
    }
    const float s = radius * 2.0f / (float) glowImage.getWidth();
    g.drawImageTransformed (glowImage, juce::AffineTransform::scale (s).translated (x - radius, y - radius), true);
}

void RenderContext::glow3D (const Vec3& p, float worldRadius, juce::Colour c, float a, float maxPx)
{
    const auto pr = project (p);
    if (! pr.visible) return;
    glow (pr.x, pr.y, std::min (maxPx, worldRadius * pr.scale), c, a);
}

void RenderContext::point (float x, float y, float radius, juce::Colour c, float a)
{
    a *= alpha;
    if (a <= 0.004f) return;
    g.setColour (c.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, a)));
    if (radius < 1.0f) g.fillRect (x - 0.5f, y - 0.5f, 1.2f, 1.2f);
    else g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
}

void RenderContext::line2D (float x1, float y1, float x2, float y2, juce::Colour c, float width, float a)
{
    a *= alpha;
    if (a <= 0.004f) return;
    g.setColour (c.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, a)));
    g.drawLine (x1, y1, x2, y2, std::max (0.4f, width));
}

bool RenderContext::clipNear (Vec3& a, Vec3& b) const
{
    const float n = cam.nearPlane() + 0.01f;
    const float da = dot (a - cam.eye(), cam.forward()), db = dot (b - cam.eye(), cam.forward());
    if (da < n && db < n) return false;
    if (da < n) a = a + (b - a) * ((n - da) / (db - da));
    else if (db < n) b = b + (a - b) * ((n - db) / (da - db));
    return true;
}

void RenderContext::line3D (Vec3 a, Vec3 b, juce::Colour c, float widthPx, float alphaV, bool glowLine)
{
    if (! clipNear (a, b)) return;
    const auto pa = project (a), pb = project (b);
    if (! pa.visible || ! pb.visible) return;
    if (glowLine) line2D (pa.x, pa.y, pb.x, pb.y, c, widthPx * (2.5f + 3.0f * bloom), alphaV * 0.18f);
    line2D (pa.x, pa.y, pb.x, pb.y, c, widthPx, alphaV);
}

void RenderContext::polyline3D (const Vec3* pts, int n, juce::Colour c, float widthPx, float a0, float a1, float widthEndScale)
{
    if (n < 2) return;
    for (int i = 0; i + 1 < n; ++i)
    {
        const float t = (float) i / (float) (n - 1);
        line3D (pts[i], pts[i + 1], c, widthPx * lerpf (1.0f, widthEndScale, t), lerpf (a0, a1, t));
    }
}

void RenderContext::ribbon3D (const Vec3* pts, const float* worldWidths, int n, juce::Colour c, float a0, float a1)
{
    if (n < 2) return;
    projScratch.resize ((size_t) n);
    for (int i = 0; i < n; ++i) projScratch[(size_t) i] = project (pts[i]);
    juce::Point<float> prevL, prevR;
    bool havePrev = false;
    for (int i = 0; i < n; ++i)
    {
        const auto& p = projScratch[(size_t) i];
        if (! p.visible) { havePrev = false; continue; }
        const auto& pp = projScratch[(size_t) std::max (0, i - 1)];
        const auto& pn = projScratch[(size_t) std::min (n - 1, i + 1)];
        float dx = (pn.visible ? pn.x : p.x) - (pp.visible ? pp.x : p.x);
        float dy = (pn.visible ? pn.y : p.y) - (pp.visible ? pp.y : p.y);
        const float len = std::sqrt (dx * dx + dy * dy);
        if (len < 1e-3f) { dx = 0; dy = 1; } else { dx /= len; dy /= len; }
        const float hw = std::max (0.35f, worldWidths[i] * p.scale * 0.5f);
        const juce::Point<float> L (p.x - dy * hw, p.y + dx * hw), R (p.x + dy * hw, p.y - dx * hw);
        if (havePrev)
        {
            const float t = (float) i / (float) (n - 1);
            const float a = lerpf (a0, a1, t) * alpha;
            if (a > 0.004f)
            {
                juce::Path quad;
                quad.startNewSubPath (prevL); quad.lineTo (L); quad.lineTo (R); quad.lineTo (prevR); quad.closeSubPath();
                g.setColour (c.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, a)));
                g.fillPath (quad);
            }
        }
        prevL = L; prevR = R; havePrev = true;
    }
}

void RenderContext::sphere (const Vec3& centre, float worldRadius, juce::Colour c, float a, Material m, float time)
{
    const auto p = project (centre);
    if (! p.visible) return;
    const float r = worldRadius * p.scale;
    if (r < 0.6f) { point (p.x, p.y, 0.8f, c, a); return; }
    a *= alpha;
    if (a <= 0.004f) return;
    const float lx = -lightDir.x * 0.38f * r, ly = -lightDir.y * 0.38f * r;
    const auto bright = c.brighter (0.6f).interpolatedWith (juce::Colours::white, 0.35f);
    switch (m)
    {
        case Material::Glass:
        {
            juce::ColourGradient grad (c.withAlpha (0.05f * a), p.x, p.y, c.withAlpha (0.55f * a), p.x + r, p.y, true);
            g.setGradientFill (grad);
            g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
            g.setColour (bright.withAlpha (0.75f * a));
            g.fillEllipse (p.x + lx - r * 0.18f, p.y + ly - r * 0.12f, r * 0.36f, r * 0.24f);
            g.setColour (bright.withAlpha (0.45f * a));
            g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, std::max (0.6f, r * 0.05f));
            break;
        }
        case Material::Hologram:
        {
            const float flick = 0.75f + 0.25f * std::sin (time * 23.0f + centre.x * 3.0f);
            g.setColour (c.withAlpha (0.12f * a * flick));
            g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
            g.setColour (c.brighter (0.4f).withAlpha (0.8f * a * flick));
            g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, std::max (0.6f, r * 0.04f));
            for (int k = 1; k < 4; ++k)
            {
                const float yy = p.y - r + 2 * r * (float) k / 4.0f, hw = std::sqrt (std::max (0.0f, r * r - (yy - p.y) * (yy - p.y)));
                g.drawLine (p.x - hw, yy, p.x + hw, yy, 0.6f);
            }
            break;
        }
        case Material::Matte:
        case Material::Metallic:
        {
            const bool metal = m == Material::Metallic;
            juce::ColourGradient grad (metal ? juce::Colours::white.withAlpha (a) : bright.withAlpha (a), p.x + lx, p.y + ly,
                                       c.darker (metal ? 1.4f : 0.9f).withAlpha (a), p.x + lx + r * 1.45f, p.y + ly, true);
            if (metal) grad.addColour (0.35, c.withAlpha (a));
            g.setGradientFill (grad);
            g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
            break;
        }
        case Material::Plasma:
        case Material::Luminous:
        default:
        {
            const auto core = m == Material::Plasma ? c.withRotatedHue (0.08f * std::sin (time * 2.0f + centre.y)) : c;
            juce::ColourGradient grad (bright.withAlpha (a), p.x + lx * 0.5f, p.y + ly * 0.5f,
                                       core.withAlpha (a * 0.35f), p.x + lx * 0.5f + r * 1.2f, p.y + ly * 0.5f, true);
            grad.addColour (0.45, core.withAlpha (a * 0.85f));
            g.setGradientFill (grad);
            g.fillEllipse (p.x - r, p.y - r, 2 * r, 2 * r);
            break;
        }
    }
}

void RenderContext::mesh (const Mesh& m, const Vec3& centre, const Vec3& scale, const Vec3& rot, const MeshStyle& st)
{
    const size_t nv = m.verts.size();
    vertScratch.resize (nv);
    projScratch.resize (nv);
    for (size_t i = 0; i < nv; ++i)
    {
        Vec3 v { m.verts[i].x * scale.x, m.verts[i].y * scale.y, m.verts[i].z * scale.z };
        if (st.distortion > 0.0f)
        {
            const Vec3 n = m.verts[i].normalised();
            const float w = noise3 (m.verts[i].x * 3.1f + st.seed * 7.0f, m.verts[i].y * 3.1f + st.time * 0.9f, m.verts[i].z * 3.1f);
            v += Vec3 { n.x * scale.x, n.y * scale.y, n.z * scale.z } * (w * st.distortion * 0.45f);
        }
        v = rotateZ (rotateX (rotateY (v, rot.y), rot.x), rot.z) + centre;
        vertScratch[i] = v;
        projScratch[i] = project (v);
    }

    const float a = st.fillAlpha * alpha;
    const float ea = st.edgeAlpha * alpha;
    const Vec3 eye = cam.eye();
    const bool opaque = st.material == Material::Matte || st.material == Material::Metallic;

    // faces (painter's algorithm, back to front)
    faceScratch.clear();
    for (int fi = 0; fi < (int) m.faces.size(); ++fi)
    {
        const auto& f = m.faces[(size_t) fi];
        bool vis = true;
        Vec3 c {};
        for (int idx : f) { vis = vis && projScratch[(size_t) idx].visible; c += vertScratch[(size_t) idx]; }
        if (! vis || f.size() < 3) continue;
        c *= 1.0f / (float) f.size();
        Vec3 n = cross (vertScratch[(size_t) f[1]] - vertScratch[(size_t) f[0]], vertScratch[(size_t) f[2]] - vertScratch[(size_t) f[0]]).normalised();
        const Vec3 toEye = (eye - c).normalised();
        float facing = dot (n, toEye);
        if (! m.closed && facing < 0.0f) { n = -n; facing = -facing; }
        if (opaque && m.closed && facing < 0.0f) continue;
        const float lambert = std::max (0.0f, dot (n, lightDir));
        faceScratch.push_back ({ dot (c - eye, cam.forward()), fi, st.ambient + (1.0f - st.ambient) * lambert, facing });
    }
    std::sort (faceScratch.begin(), faceScratch.end(), [] (const FaceRef& x, const FaceRef& y) { return x.depth > y.depth; });

    for (const auto& fr : faceScratch)
    {
        const auto& f = m.faces[(size_t) fr.index];
        juce::Path path;
        path.startNewSubPath (projScratch[(size_t) f[0]].x, projScratch[(size_t) f[0]].y);
        for (size_t k = 1; k < f.size(); ++k) path.lineTo (projScratch[(size_t) f[k]].x, projScratch[(size_t) f[k]].y);
        path.closeSubPath();

        juce::Colour col = st.colour;
        float fa = a;
        switch (st.material)
        {
            case Material::Luminous: col = col.withMultipliedBrightness (0.55f + 0.6f * fr.shade * st.emission); fa *= 0.55f; break;
            case Material::Glass:
            {
                const float fres = 1.0f - std::abs (fr.facing);
                col = col.interpolatedWith (juce::Colours::white, 0.25f + 0.5f * fres * fres);
                fa *= 0.14f + 0.45f * fres;
                break;
            }
            case Material::Matte:    col = col.withMultipliedBrightness (0.25f + 0.85f * fr.shade); fa = std::min (1.0f, fa * 1.4f); break;
            case Material::Metallic:
            {
                const float s = fr.shade * fr.shade;
                col = col.withMultipliedBrightness (0.15f + 0.9f * s).interpolatedWith (juce::Colours::white, s > 0.8f ? (s - 0.8f) * 3.0f : 0.0f);
                fa = std::min (1.0f, fa * 1.4f);
                break;
            }
            case Material::Plasma:   col = col.withRotatedHue (0.06f * std::sin (st.time * 3.0f + (float) fr.index)).withMultipliedBrightness (0.7f + 0.5f * fr.shade); fa *= 0.5f; break;
            case Material::Hologram: fa *= 0.07f; break;
        }
        if (fa > 0.004f)
        {
            g.setColour (col.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, fa)));
            g.fillPath (path);
        }
    }

    // edges
    const bool drawEdges = m.faces.empty() || st.material != Material::Matte;
    if (drawEdges && ea > 0.004f)
    {
        juce::Colour ec = st.colour.brighter (st.material == Material::Glass ? 0.8f : 0.35f);
        float ew = st.edgeWidth;
        float eAlpha = ea;
        if (st.material == Material::Hologram) eAlpha *= 0.65f + 0.35f * std::sin (st.time * 19.0f + st.seed * 10.0f);
        if (st.material == Material::Metallic) eAlpha *= 0.35f;
        g.setColour (ec.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, eAlpha)));
        for (const auto& e : m.edges)
        {
            const auto& p0 = projScratch[(size_t) e.first];
            const auto& p1 = projScratch[(size_t) e.second];
            if (p0.visible && p1.visible) g.drawLine (p0.x, p0.y, p1.x, p1.y, ew);
        }
    }
}

void RenderContext::text3D (const Vec3& p, const juce::String& s, juce::Colour c, float a, float height, juce::Justification j)
{
    const auto pr = project (p);
    if (! pr.visible || a * alpha < 0.01f) return;
    g.setColour (c.withMultipliedAlpha (juce::jlimit (0.0f, 1.0f, a * alpha)));
    g.setFont (ui::Fonts::get().medium (height));
    g.drawText (s, juce::Rectangle<float> (pr.x - 120, pr.y - height, 240, height * 2), j, false);
}
} // namespace eidolon
