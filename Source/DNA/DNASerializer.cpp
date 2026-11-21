#include "DNASerializer.h"

namespace eidolon
{
juce::var paramsToVar (const ParamSet& p, bool dnaOnly)
{
    auto* obj = new juce::DynamicObject();
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfo (i);
        if (dnaOnly && ! info.isDNA()) continue;
        obj->setProperty (juce::Identifier (info.key), (double) p.v[(size_t) i]);
    }
    return juce::var (obj);
}

int paramsFromVar (const juce::var& v, ParamSet& p, bool dnaOnly)
{
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) return 0;
    int applied = 0;
    for (const auto& prop : obj->getProperties())
    {
        const int idx = findParamByKey (prop.name.toString().toStdString());
        if (idx < 0) continue;                                  // unknown (newer version / removed): ignore
        if (dnaOnly && ! paramInfo (idx).isDNA()) continue;
        if (! (prop.value.isDouble() || prop.value.isInt() || prop.value.isInt64() || prop.value.isBool())) continue;
        p.set ((P) idx, (float) (double) prop.value);           // sanitised: clamped + quantized
        ++applied;
    }
    return applied;
}

juce::var mappingToVar (const Mapping& m)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("enabled", m.enabled);
    o->setProperty ("source", (int) m.source);
    o->setProperty ("sourceName", mapSourceName (m.source));
    o->setProperty ("cc", m.ccNumber);
    o->setProperty ("channel", m.channel);
    o->setProperty ("target", juce::String (paramInfo (m.target).key));
    o->setProperty ("outMin", m.outMin);
    o->setProperty ("outMax", m.outMax);
    o->setProperty ("inMin", m.inMin);
    o->setProperty ("inMax", m.inMax);
    o->setProperty ("deadZone", m.deadZone);
    o->setProperty ("sensitivity", m.sensitivity);
    o->setProperty ("invert", m.invert);
    o->setProperty ("curve", (int) m.curve);
    o->setProperty ("curveAmount", m.curveAmount);
    juce::Array<juce::var> pts;
    for (float c : m.customPoints) pts.add (c);
    o->setProperty ("customPoints", pts);
    o->setProperty ("response", (int) m.response);
    o->setProperty ("decay", m.decay);
    o->setProperty ("smoothing", m.smoothing);
    o->setProperty ("blend", (int) m.blend);
    return juce::var (o);
}

bool mappingFromVar (const juce::var& v, Mapping& m)
{
    if (! v.isObject()) return false;
    auto num = [&] (const char* k, float def, float lo, float hi)
    {
        const auto x = v.getProperty (k, def);
        const float f = (x.isDouble() || x.isInt() || x.isInt64() || x.isBool()) ? (float) (double) x : def;
        return std::isfinite (f) ? juce::jlimit (lo, hi, f) : def;
    };
    const int target = findParamByKey (v.getProperty ("target", "").toString().toStdString());
    if (target < 0 || ! paramInfo (target).isModulatable()) return false;
    m = Mapping {};
    m.target = target;
    m.enabled = (bool) v.getProperty ("enabled", true);
    m.source = (MapSource) (int) num ("source", 2, 0, (float) ((int) MapSource::Count - 1));
    m.ccNumber = (int) num ("cc", 1, 0, 127);
    m.channel = (int) num ("channel", 0, 0, 16);
    m.outMin = num ("outMin", 0, 0, 1);
    m.outMax = num ("outMax", 1, 0, 1);
    m.inMin = num ("inMin", 0, 0, 1);
    m.inMax = num ("inMax", 1, 0, 1);
    m.deadZone = num ("deadZone", 0, 0, 0.95f);
    m.sensitivity = num ("sensitivity", 1, 0.05f, 8);
    m.invert = (bool) v.getProperty ("invert", false);
    m.curve = (MapCurve) (int) num ("curve", 0, 0, (float) ((int) MapCurve::Count - 1));
    m.curveAmount = num ("curveAmount", 0.5f, 0, 1);
    if (auto* arr = v.getProperty ("customPoints", {}).getArray())
        for (int i = 0; i < 3 && i < arr->size(); ++i) m.customPoints[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) (double) (*arr)[i]);
    m.response = (MapResponse) (int) num ("response", 0, 0, (float) ((int) MapResponse::Count - 1));
    m.decay = num ("decay", 0.5f, 0.02f, 10);
    m.smoothing = num ("smoothing", 0.15f, 0, 1);
    m.blend = (MapBlend) (int) num ("blend", 0, 0, (float) ((int) MapBlend::Count - 1));
    return true;
}

juce::var dnaToVar (const VisualDNA& d)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("format", "EIDOLON-VisualDNA");
    o->setProperty ("version", kDNAFormatVersion);
    o->setProperty ("name", juce::String (d.name));
    o->setProperty ("category", juce::String (d.category));
    o->setProperty ("author", juce::String (d.author));
    o->setProperty ("description", juce::String (d.description));
    o->setProperty ("seed", (juce::int64) d.seed);
    o->setProperty ("params", paramsToVar (d.params, true));
    juce::Array<juce::var> maps;
    for (const auto& m : d.mappings) maps.add (mappingToVar (m));
    o->setProperty ("mappings", maps);
    return juce::var (o);
}

bool dnaFromVar (const juce::var& v, VisualDNA& out, juce::String& error)
{
    if (! v.isObject()) { error = "Not a JSON object"; return false; }
    if (v.getProperty ("format", "").toString() != "EIDOLON-VisualDNA") { error = "Not an EIDOLON Visual DNA file"; return false; }
    const int version = (int) v.getProperty ("version", 0);
    if (version < 1) { error = "Unsupported Visual DNA version"; return false; }
    VisualDNA d;   // defaults for anything missing
    d.name = v.getProperty ("name", "Untitled").toString().substring (0, 80).toStdString();
    d.category = v.getProperty ("category", "Custom").toString().substring (0, 40).toStdString();
    d.author = v.getProperty ("author", "").toString().substring (0, 80).toStdString();
    d.description = v.getProperty ("description", "").toString().substring (0, 500).toStdString();
    d.seed = (uint32_t) (juce::int64) v.getProperty ("seed", 1);
    paramsFromVar (v.getProperty ("params", {}), d.params, true);
    if (auto* arr = v.getProperty ("mappings", {}).getArray())
        for (const auto& mv : *arr)
        {
            Mapping m;
            if (mappingFromVar (mv, m)) d.mappings.push_back (m);
            if (d.mappings.size() >= 64) break;
        }
    out = d;
    return true;
}

juce::String dnaToJson (const VisualDNA& d) { return juce::JSON::toString (dnaToVar (d), false); }

bool dnaFromJson (const juce::String& json, VisualDNA& out, juce::String& error)
{
    juce::var v;
    const auto result = juce::JSON::parse (json, v);
    if (result.failed()) { error = "Invalid JSON: " + result.getErrorMessage(); return false; }
    return dnaFromVar (v, out, error);
}
} // namespace eidolon
