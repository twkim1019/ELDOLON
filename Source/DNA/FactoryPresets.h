#pragma once
#include "VisualDNA.h"
#include <vector>

namespace eidolon
{
/** Built-in Visual DNA presets (read-only; generated in code so they can never go missing). */
const std::vector<VisualDNA>& factoryPresets();
VisualDNA defaultDNA();
} // namespace eidolon
