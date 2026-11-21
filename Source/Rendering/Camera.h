#pragma once
// EIDOLON — perspective camera + musical camera controller (static / orbit / follow / follow active notes /
// follow musical center / cinematic / automatic), with smooth damping and user orbit / pan / zoom.
#include "../Core/MathUtil.h"
#include "../Core/Params.h"
#include <juce_graphics/juce_graphics.h>

namespace eidolon
{
struct Projected
{
    float x = 0, y = 0;     // screen pixels
    float depth = 0;        // distance along the view axis
    float scale = 0;        // pixels per world unit at this depth
    bool visible = false;
};

class Camera
{
public:
    Vec3 target { 0, 0, 0 };
    float yaw = -0.5f, pitch = 0.2f, roll = 0.0f, distance = 14.0f, fovDeg = 55.0f;

    void setViewport (juce::Rectangle<float> area);
    Projected project (const Vec3& p) const;
    Vec3 eye() const { return eyePos; }
    Vec3 forward() const { return fwd; }
    Vec3 rightVec() const { return right; }
    Vec3 upVec() const { return up; }
    juce::Rectangle<float> viewport() const { return area; }
    float focalLength() const { return focal; }
    float nearPlane() const { return 0.3f; }

private:
    juce::Rectangle<float> area { 0, 0, 800, 500 };
    Vec3 eyePos, fwd { 0, 0, 1 }, right { 1, 0, 0 }, up { 0, 1, 0 };
    float focal = 500.0f;
};

struct CameraFocus
{
    Vec3 activeCentroid;     // centroid of sounding note objects
    Vec3 latest;             // newest note object
    float activeSpread = 4;  // radius of sounding objects
    bool hasActive = false;
    float musicalCenterY = 0;
    float energy = 0;        // 0..1 musical activity
    bool chordChanged = false;
    bool sceneChanged = false;
    float impulse = 0;       // velocity hit for camera impact
};

class CameraController
{
public:
    void update (float dt, float time, const ParamSet& p, const CameraFocus& f, Camera& cam);
    void reset();
    void drag (float dx, float dy);      // orbit (pixels)
    void pan (float dx, float dy, const Camera& cam);
    void zoom (float wheelDelta);
    bool userControlled() const { return userTime > 0.0f; }

private:
    float userYaw = 0, userPitch = 0, userZoom = 1.0f, userTime = 0.0f;
    Vec3 userPan;
    float orbitAngle = 0, cineTime = 0, shotYaw = 0, shotPitch = 0, shotDist = 1, shotTimer = 0;
    float shake = 0;
    Vec3 smoothTarget;
    float sYaw = -0.5f, sPitch = 0.2f, sDist = 14, sFov = 55;
    bool initialised = false;
};
} // namespace eidolon
