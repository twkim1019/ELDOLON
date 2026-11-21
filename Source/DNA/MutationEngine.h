#pragma once
// EIDOLON — controlled, reproducible procedural variation of Visual DNA.
// Same input DNA + same seed + same settings => bit-identical result on every platform.
#include "VisualDNA.h"
#include <array>
#include <vector>

namespace eidolon
{
struct MutationSettings
{
    float intensity = 0.3f;                              // 0..1
    uint32_t seed = 1;
    std::array<bool, (size_t) MG::Count> preserve {};    // true = keep this group untouched
    bool mutateMappings = true;                          // "Visual mapping" target
    bool preserveMappings = false;
};

class MutationEngine
{
public:
    static VisualDNA mutate (const VisualDNA& base, const MutationSettings& s);
    static std::vector<VisualDNA> variations (const VisualDNA& base, const MutationSettings& s, int count);
    static VisualDNA randomize (uint32_t seed, const VisualDNA& base, const std::array<bool, (size_t) MG::Count>& preserve);
};

/** Bounded snapshot history for undo / redo of parameter edits, preset loads and mutations. */
class DNAHistory
{
public:
    void push (const VisualDNA& d);
    bool canUndo() const { return index > 0; }
    bool canRedo() const { return index + 1 < (int) stack.size(); }
    const VisualDNA* undo() { return canUndo() ? &stack[(size_t) --index] : nullptr; }
    const VisualDNA* redo() { return canRedo() ? &stack[(size_t) ++index] : nullptr; }
    void reset (const VisualDNA& d) { stack.clear(); stack.push_back (d); index = 0; }
    int size() const { return (int) stack.size(); }

private:
    std::vector<VisualDNA> stack;
    int index = -1;
    static constexpr size_t kMax = 64;
};
} // namespace eidolon
