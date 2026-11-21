#pragma once
// EIDOLON — procedural meshes for note geometry (unit size, centred at the origin).
#include "../Core/MathUtil.h"
#include <vector>

namespace eidolon
{
enum class Shape : int { Sphere, Cube, Ring, Crystal, Wireframe, Particle, Ribbon, Cylinder, Polygon, Torus, Abstract, Count };

struct Mesh
{
    std::vector<Vec3> verts;
    std::vector<std::pair<int, int>> edges;
    std::vector<std::vector<int>> faces;     // polygon index lists, counter-clockwise from outside
    bool closed = true;                      // false: never back-face cull (rings, flat shapes)
};

/** Cached mesh for (shape, detail). detail 3..12. Thread: message / render thread only. */
const Mesh& meshFor (Shape s, int detail);
} // namespace eidolon
