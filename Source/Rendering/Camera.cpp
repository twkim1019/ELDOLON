#include "Camera.h"

namespace eidolon
{
void Camera::setViewport (juce::Rectangle<float> a)
{
    area = a;
    const float fov = clampv (fovDeg, 10.0f, 150.0f) * kPi / 180.0f;
    focal = (area.getHeight() * 0.5f) / std::tan (fov * 0.5f);
    const float cp = std::cos (pitch);
    eyePos = target + Vec3 { cp * std::sin (yaw), std::sin (pitch), cp * std::cos (yaw) } * distance;
    fwd = (target - eyePos).normalised();
    right = cross (fwd, { 0, 1, 0 }).normalised();
    if (right.length() < 0.5f) right = { 1, 0, 0 };
    up = cross (right, fwd).normalised();
    if (roll != 0.0f)
    {
        const float c = std::cos (roll), s = std::sin (roll);
        const Vec3 r2 = right * c + up * s, u2 = up * c - right * s;
        right = r2; up = u2;
    }
}

Projected Camera::project (const Vec3& p) const
{
    Projected r;
    const Vec3 v = p - eyePos;
    const float z = dot (v, fwd);
    r.depth = z;
    if (z < nearPlane()) return r;
    const float inv = focal / z;
    r.x = area.getCentreX() + dot (v, right) * inv;
    r.y = area.getCentreY() - dot (v, up) * inv;
    r.scale = inv;
    r.visible = true;
    return r;
}

void CameraController::reset()
{
    userYaw = userPitch = 0.0f;
    userZoom = 1.0f;
    userPan = {};
    userTime = 0.0f;
    orbitAngle = 0.0f;
}

void CameraController::drag (float dx, float dy)
{
    userYaw -= dx * 0.006f;
    userPitch = clampv (userPitch + dy * 0.005f, -1.3f, 1.3f);
    userTime = 4.0f;
}

void CameraController::pan (float dx, float dy, const Camera& cam)
{
    const float k = cam.distance / std::max (1.0f, cam.focalLength());
    userPan -= cam.rightVec() * (dx * k);
    userPan += cam.upVec() * (dy * k);
    userTime = 4.0f;
}

void CameraController::zoom (float wheelDelta)
{
    userZoom = clampv (userZoom * std::exp (-wheelDelta * 0.6f), 0.15f, 5.0f);
    userTime = 4.0f;
}

void CameraController::update (float dt, float time, const ParamSet& p, const CameraFocus& f, Camera& cam)
{
    const int mode = p.geti (P::CamMode);
    float yaw = p.get (P::CamYaw) * kPi / 180.0f;
    float pitch = p.get (P::CamPitch) * kPi / 180.0f;
    float dist = p.get (P::CamDistance);
    const float fov = p.get (P::CamFov);
    const float speed = p.get (P::CamSpeed);
    const float react = p.get (P::CamReact);
    Vec3 target {};
    userTime = std::max (0.0f, userTime - dt);

    switch (mode)
    {
        case 0: break;   // static
        case 1: orbitAngle += dt * speed * 0.25f; yaw += orbitAngle; break;
        case 2: target = f.latest * 0.65f; break;
        case 3:
            if (f.hasActive) { target = f.activeCentroid; dist = dist * clampv ((f.activeSpread * 2.2f + 5.0f) / 12.0f, 0.6f, 2.2f); }
            break;
        case 4: target.y = f.musicalCenterY; break;
        case 5:
        {
            cineTime += dt * (0.4f + speed);
            shotTimer -= dt;
            if (f.chordChanged || f.sceneChanged || shotTimer <= 0.0f)
            {
                const float h = hash11 (cineTime * 3.7f + 1.3f), h2 = hash11 (cineTime * 1.9f + 7.1f), h3 = hash11 (cineTime * 5.3f);
                shotYaw = h * 0.9f; shotPitch = h2 * 0.25f; shotDist = 1.0f + h3 * 0.3f;
                shotTimer = 6.0f + 4.0f * std::abs (h);
            }
            yaw += std::sin (cineTime * 0.11f) * 0.5f + shotYaw;
            pitch += std::sin (cineTime * 0.19f) * 0.12f + shotPitch;
            dist *= shotDist * (1.0f + 0.15f * std::sin (cineTime * 0.17f));
            target.y = f.musicalCenterY * 0.5f;
            break;
        }
        case 6:
        default:
        {
            orbitAngle += dt * speed * (0.05f + 0.3f * f.energy);
            yaw += orbitAngle;
            pitch += 0.08f * std::sin (time * 0.07f);
            target.y = f.musicalCenterY * 0.6f;
            dist *= lerpf (0.85f, 1.2f, f.energy);
            break;
        }
    }

    if (mode != 0)
    {
        target.y += f.musicalCenterY * react * 0.25f;
        dist *= 1.0f + f.energy * react * 0.15f;
    }

    shake = std::max (shake * std::exp (-dt * 7.0f), f.impulse * p.get (P::CamShake));
    target += Vec3 { noise1 (time * 13.0f), noise1 (time * 11.0f + 5.0f), 0.0f } * (shake * 0.35f);

    yaw += userYaw;
    pitch = clampv (pitch + userPitch, -1.45f, 1.45f);
    dist = clampv (dist * userZoom, 1.5f, 120.0f);
    target += userPan;

    const float tau = 0.04f + p.get (P::CamSmooth) * 1.1f;
    const float k = initialised ? smoothCoef (dt, tau) : 1.0f;
    float dy = yaw - sYaw;
    while (dy > kPi) dy -= kTau;
    while (dy < -kPi) dy += kTau;
    sYaw += dy * k;
    sPitch += (pitch - sPitch) * k;
    sDist += (dist - sDist) * k;
    sFov += (fov - sFov) * k;
    smoothTarget += (target - smoothTarget) * k;
    initialised = true;

    cam.yaw = sYaw;
    cam.pitch = sPitch;
    cam.distance = sDist;
    cam.fovDeg = sFov;
    cam.target = smoothTarget;
    cam.roll = mode == 5 ? 0.03f * std::sin (cineTime * 0.13f) : 0.0f;
}
} // namespace eidolon
