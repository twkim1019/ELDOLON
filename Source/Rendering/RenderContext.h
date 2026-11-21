#pragma once
// EIDOLON — 3D drawing primitives on top of juce::Graphics. The same code renders through JUCE's
// OpenGL renderer (GPU, when the canvas has an attached OpenGLContext) or the software renderer (fallback).
#include "Camera.h"
#include "Geometry.h"
#include <juce_graphics/juce_graphics.h>

namespace eidolon
{
enum class Material : int { Luminous, Glass, Matte, Metallic, Plasma, Hologram };

struct MeshStyle
{
    juce::Colour colour { juce::Colours::white };
    float fillAlpha = 0.6f, edgeAlpha = 0.8f, edgeWidth = 1.0f;
    Material material = Material::Luminous;
    float distortion = 0.0f, time = 0.0f, seed = 0.0f, ambient = 0.35f, emission = 1.0f;
};

class RenderContext
{
public:
    RenderContext (juce::Graphics& g, const Camera& cam, const juce::Image& glowImage);

    juce::Graphics& g;
    const Camera& cam;
    const juce::Image& glowImage;
    float alpha = 1.0f;        // layer opacity multiplier
    float bloom = 0.6f;        // glow spread
    float detail = 1.0f;       // quality budget 0..1
    float brightness = 1.0f;
    float texture = 0.0f;
    Vec3 lightDir { 0.4f, 0.8f, 0.45f };
    juce::Rectangle<float> area;

    Projected project (const Vec3& p) const { return cam.project (p); }
    void glow (float x, float y, float radius, juce::Colour c, float a);
    void glow3D (const Vec3& p, float worldRadius, juce::Colour c, float a, float maxPx = 600.0f);
    void point (float x, float y, float radius, juce::Colour c, float a);
    void line2D (float x1, float y1, float x2, float y2, juce::Colour c, float width, float a);
    void line3D (Vec3 a, Vec3 b, juce::Colour c, float widthPx, float alphaV, bool glowLine = false);
    void polyline3D (const Vec3* pts, int n, juce::Colour c, float widthPx, float a0, float a1, float widthEndScale = 1.0f);
    void ribbon3D (const Vec3* pts, const float* worldWidths, int n, juce::Colour c, float a0, float a1);
    void sphere (const Vec3& centre, float worldRadius, juce::Colour c, float a, Material m, float time);
    void mesh (const Mesh& m, const Vec3& centre, const Vec3& scale, const Vec3& rot, const MeshStyle& st);
    void text3D (const Vec3& p, const juce::String& s, juce::Colour c, float a, float height, juce::Justification j = juce::Justification::centred);

    /** Clips a segment against the near plane. Returns false if completely behind. */
    bool clipNear (Vec3& a, Vec3& b) const;

    static juce::Image createGlowImage (int size = 64);

private:
    std::vector<Projected> projScratch;
    std::vector<Vec3> vertScratch;
    struct FaceRef { float depth; int index; float shade; float facing; };
    std::vector<FaceRef> faceScratch;
};
} // namespace eidolon
