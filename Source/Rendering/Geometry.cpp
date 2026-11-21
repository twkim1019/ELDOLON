#include "Geometry.h"
#include <map>
#include <memory>

namespace eidolon
{
namespace
{
void addEdgesFromFaces (Mesh& m)
{
    for (const auto& f : m.faces)
        for (size_t i = 0; i < f.size(); ++i)
        {
            int a = f[i], b = f[(i + 1) % f.size()];
            if (a > b) std::swap (a, b);
            bool dup = false;
            for (auto& e : m.edges) if (e.first == a && e.second == b) { dup = true; break; }
            if (! dup) m.edges.push_back ({ a, b });
        }
}

Mesh cube()
{
    Mesh m;
    for (int i = 0; i < 8; ++i) m.verts.push_back ({ (i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f });
    m.faces = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
    addEdgesFromFaces (m);
    return m;
}

Mesh icosahedron (float spike)
{
    Mesh m;
    const float t = (1.0f + std::sqrt (5.0f)) * 0.5f;
    const Vec3 v[] = { { -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 }, { 0, -1, t }, { 0, 1, t },
                       { 0, -1, -t }, { 0, 1, -t }, { t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 } };
    for (auto p : v) m.verts.push_back (p.normalised() * 0.5f);
    m.faces = { { 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 }, { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 },
                { 10, 7, 6 }, { 7, 1, 8 }, { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 }, { 4, 9, 5 },
                { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 } };
    if (spike > 0.0f)
    {
        // stellate: raise a spike on every face
        const auto base = m.faces;
        m.faces.clear();
        for (const auto& f : base)
        {
            const Vec3 c = (m.verts[(size_t) f[0]] + m.verts[(size_t) f[1]] + m.verts[(size_t) f[2]]) * (1.0f / 3.0f);
            m.verts.push_back (c.normalised() * (0.5f + spike));
            const int s = (int) m.verts.size() - 1;
            m.faces.push_back ({ f[0], f[1], s });
            m.faces.push_back ({ f[1], f[2], s });
            m.faces.push_back ({ f[2], f[0], s });
        }
    }
    addEdgesFromFaces (m);
    return m;
}

Mesh bipyramid (int sides)
{
    Mesh m;
    m.verts.push_back ({ 0, 0.75f, 0 });
    m.verts.push_back ({ 0, -0.75f, 0 });
    for (int i = 0; i < sides; ++i)
    {
        const float a = kTau * (float) i / (float) sides;
        m.verts.push_back ({ 0.42f * std::cos (a), 0.0f, 0.42f * std::sin (a) });
    }
    for (int i = 0; i < sides; ++i)
    {
        const int a = 2 + i, b = 2 + (i + 1) % sides;
        m.faces.push_back ({ 0, b, a });
        m.faces.push_back ({ 1, a, b });
    }
    addEdgesFromFaces (m);
    return m;
}

Mesh prism (int sides, float length)
{
    Mesh m;
    for (int end = 0; end < 2; ++end)
        for (int i = 0; i < sides; ++i)
        {
            const float a = kTau * (float) i / (float) sides;
            m.verts.push_back ({ end ? length * 0.5f : -length * 0.5f, 0.5f * std::cos (a), 0.5f * std::sin (a) });
        }
    std::vector<int> capA, capB;
    for (int i = 0; i < sides; ++i)
    {
        const int a = i, b = (i + 1) % sides;
        m.faces.push_back ({ a, b, sides + b, sides + a });
        capA.push_back (sides - 1 - i);
        capB.push_back (sides + i);
    }
    m.faces.push_back (capA);
    m.faces.push_back (capB);
    addEdgesFromFaces (m);
    return m;
}

Mesh ring (int segments, bool flatFilled)
{
    Mesh m;
    m.closed = false;
    std::vector<int> poly;
    for (int i = 0; i < segments; ++i)
    {
        const float a = kTau * (float) i / (float) segments;
        m.verts.push_back ({ 0.5f * std::cos (a), 0.5f * std::sin (a), 0.0f });
        poly.push_back (i);
        m.edges.push_back ({ i, (i + 1) % segments });
    }
    if (flatFilled) m.faces.push_back (poly);
    return m;
}

Mesh torus (int major, int minor)
{
    Mesh m;
    for (int i = 0; i < major; ++i)
        for (int j = 0; j < minor; ++j)
        {
            const float u = kTau * (float) i / (float) major, v = kTau * (float) j / (float) minor;
            const float r = 0.36f + 0.14f * std::cos (v);
            m.verts.push_back ({ r * std::cos (u), 0.14f * std::sin (v), r * std::sin (u) });
        }
    for (int i = 0; i < major; ++i)
        for (int j = 0; j < minor; ++j)
        {
            const int a = i * minor + j, b = ((i + 1) % major) * minor + j;
            const int c = ((i + 1) % major) * minor + (j + 1) % minor, d = i * minor + (j + 1) % minor;
            m.faces.push_back ({ a, d, c, b });
        }
    addEdgesFromFaces (m);
    return m;
}

Mesh build (Shape s, int detail)
{
    switch (s)
    {
        case Shape::Cube:      return cube();
        case Shape::Ring:      return ring (std::max (12, detail * 4), false);
        case Shape::Crystal:   return bipyramid (std::max (3, detail));
        case Shape::Wireframe: return icosahedron (0.0f);
        case Shape::Cylinder:  return prism (std::max (3, detail), 1.0f);
        case Shape::Polygon:   return ring (std::max (3, detail), true);
        case Shape::Torus:     return torus (std::max (8, detail * 2), std::max (5, detail));
        case Shape::Abstract:  return icosahedron (0.35f);
        case Shape::Sphere:
        case Shape::Particle:
        case Shape::Ribbon:
        case Shape::Count:     break;
    }
    return icosahedron (0.0f);
}
} // namespace

const Mesh& meshFor (Shape s, int detail)
{
    static std::map<int, std::unique_ptr<Mesh>> cache;
    detail = clampv (detail, 3, 12);
    const int key = (int) s * 64 + detail;
    auto it = cache.find (key);
    if (it != cache.end()) return *it->second;
    auto m = std::make_unique<Mesh> (build (s, detail));
    const Mesh& ref = *m;
    cache.emplace (key, std::move (m));
    return ref;
}
} // namespace eidolon
