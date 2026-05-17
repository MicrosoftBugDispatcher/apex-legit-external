#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include "../memory/comms.hpp"
#include "../sdk/sdk.hpp"
#include "../entities/entity.hpp"
#include "../config/config.h"
#include "aimbot.h"

float NormalizeAngle(float a)
{
    while (a >  180.f) a -= 360.f;
    while (a < -180.f) a += 360.f;
    return a;
}

Vector CalcAngle(Vector src, Vector dst)
{
    Vector delta;
    delta.x = dst.x - src.x;
    delta.y = dst.y - src.y;
    delta.z = dst.z - src.z;

    float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (hyp < 0.001f) hyp = 0.001f;
    Vector angle;
    angle.x = -(atanf(delta.z / hyp) * (180.f / 3.14159265358979323846f));
    angle.y = atan2f(delta.y, delta.x) * (180.f / 3.14159265358979323846f);
    angle.z = 0.0f;

    while (angle.y >  180.f) angle.y -= 360.f;
    while (angle.y < -180.f) angle.y += 360.f;

    return angle;
}

Vector GetClosestTargetBone(Kernel::Driver* drv, uintptr_t localPlayer, uintptr_t& outEntity, int screenW, int screenH)
{
    (void)screenW;
    (void)screenH;

    if (!localPlayer || localPlayer <= 0x1000) {
        outEntity = 0;
        return Vector();
    }

    if (outEntity) {
        int health = drv->Read<int>(outEntity + offsets::m_iHealth);
        int entTeam = drv->Read<int>(outEntity + offsets::m_iTeamNum);
        int localTeam = drv->Read<int>(localPlayer + offsets::m_iTeamNum);

        if (health > 0 && health <= 500 && entTeam != localTeam) {
            Vector headBone = GetBonePositionByHitbox(drv, outEntity, 0);
            if (headBone.x != 0 || headBone.y != 0 || headBone.z != 0)
                return headBone;
            Vector origin = GetEntityOrigin(drv, outEntity);
            if (origin.x != 0 || origin.y != 0 || origin.z != 0)
                return Vector(origin.x, origin.y, origin.z + 60.f);
        }
        outEntity = 0;
    }

    Vector cam = GetCameraPosition(drv, localPlayer);
    Vector viewAng;
    viewAng.x = drv->Read<float>(localPlayer + offsets::viewangles);
    viewAng.y = drv->Read<float>(localPlayer + offsets::viewangles + 0x4);

    float bestAngDist = FLT_MAX;
    Vector bestBone = Vector();

    int localTeam = drv->Read<int>(localPlayer + offsets::m_iTeamNum);

    for (int i = 0; i < g_max_entities; i++) {
        uintptr_t ent = GetEntityPtr(drv, i);
        if (!ent || ent <= 0x1000 || ent == localPlayer) continue;

        int health = drv->Read<int>(ent + offsets::m_iHealth);
        if (health <= 0 || health > 500) continue;

        int entTeam = drv->Read<int>(ent + offsets::m_iTeamNum);
        if (entTeam == localTeam) continue;

        Vector aimPos = GetBonePositionByHitbox(drv, ent, 0);
        if (aimPos.x == 0 && aimPos.y == 0 && aimPos.z == 0) {
            Vector origin = GetEntityOrigin(drv, ent);
            if (origin.x == 0 && origin.y == 0 && origin.z == 0) continue;
            aimPos = Vector(origin.x, origin.y, origin.z + 60.f);
        }

        Vector angleToEnt = CalcAngle(cam, aimPos);
        float dX = NormalizeAngle(angleToEnt.x - viewAng.x);
        float dY = NormalizeAngle(angleToEnt.y - viewAng.y);
        float angDist = sqrtf(dX * dX + dY * dY);

        if (g_aim_fov > 0.f && angDist > g_aim_fov)
            continue;

        if (angDist < bestAngDist) {
            bestAngDist = angDist;
            bestBone = aimPos;
            outEntity = ent;
        }
    }

    return bestBone;
}
