#pragma once
// EIDOLON — JSON serialization of Visual DNA and parameter sets (versioned, validated, forward compatible:
// unknown keys are ignored, missing keys keep their defaults, every value is range-checked).
#include "VisualDNA.h"
#include <juce_core/juce_core.h>

namespace eidolon
{
juce::var paramsToVar (const ParamSet& p, bool dnaOnly);
int paramsFromVar (const juce::var& v, ParamSet& p, bool dnaOnly);   // returns number of keys applied

juce::var mappingToVar (const Mapping& m);
bool mappingFromVar (const juce::var& v, Mapping& m);

juce::var dnaToVar (const VisualDNA& d);
bool dnaFromVar (const juce::var& v, VisualDNA& out, juce::String& error);

juce::String dnaToJson (const VisualDNA& d);
bool dnaFromJson (const juce::String& json, VisualDNA& out, juce::String& error);
} // namespace eidolon
