#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../sdk/sdk.hpp"

inline uintptr_t GetEntityPtr(Kernel::Driver* drv, int i)
{
    return drv->Read<uintptr_t>(drv->Base + offsets::entity_list + ((uintptr_t)i << 5));
}

inline uintptr_t GetEntityBoneArray(Kernel::Driver* drv, uintptr_t entity)
{
    uintptr_t bone = drv->Read<uintptr_t>(entity + offsets::bone_array);
    if (!bone || bone <= 0x1000) return 0;
    return bone;
}

inline Vector GetEntityOrigin(Kernel::Driver* drv, uintptr_t entity)
{
    return drv->Read<Vector>(entity + offsets::m_vecAbsOrigin);
}

inline Vector GetCameraPosition(Kernel::Driver* drv, uintptr_t entity)
{
    return drv->Read<Vector>(entity + offsets::camera_origin);
}

inline Vector GetBonePositionByHitbox(Kernel::Driver* drv, uintptr_t entity, int id)
{
    Vector origin = drv->Read<Vector>(entity + offsets::m_vecAbsOrigin);

    uint64_t Model = drv->Read<uint64_t>(entity + offsets::studiohdr);
    if (!Model || Model <= 0x1000 || Model > 0x7FFFFFFFFFFFFFFF) return Vector();

    uint64_t StudioHdr = drv->Read<uint64_t>(Model + 0x8);
    if (!StudioHdr || StudioHdr <= 0x1000 || StudioHdr > 0x7FFFFFFFFFFFFFFF) return Vector();

    uint16_t HitboxCache = drv->Read<uint16_t>(StudioHdr + 0x34);
    uint64_t HitBoxsArray = StudioHdr + ((uint16_t)(HitboxCache & 0xFFFE) << (4 * (HitboxCache & 1)));
    if (HitBoxsArray <= StudioHdr) return Vector();

    uint16_t IndexCache = drv->Read<uint16_t>(HitBoxsArray + 0x4);
    int HitboxIndex = ((uint16_t)(IndexCache & 0xFFFE) << (4 * (IndexCache & 1)));

    uint16_t Bone = drv->Read<uint16_t>(HitBoxsArray + HitboxIndex + (id * 0x20));
    if (Bone == 0 || Bone > 255) return Vector();

    uint64_t BoneArray = GetEntityBoneArray(drv, entity);
    if (!BoneArray) return Vector();

    matrix3x4_t Matrix = drv->Read<matrix3x4_t>(BoneArray + Bone * sizeof(matrix3x4_t));

    return Vector(
        Matrix.m_flMatVal[0][3] + origin.x,
        Matrix.m_flMatVal[1][3] + origin.y,
        Matrix.m_flMatVal[2][3] + origin.z
    );
}

inline std::string ReadEntityName(Kernel::Driver* drv, uintptr_t addr)
{
    if (!addr) return {};
    uintptr_t namePtr = drv->Read<uintptr_t>(addr + offsets::m_iSignifierName);
    if (!namePtr || namePtr <= 0x1000) return {};
    std::string s;
    s.reserve(32);
    for (int i = 0; i < 64; i++) {
        uint16_t c = drv->Read<uint16_t>(namePtr + static_cast<uintptr_t>(i) * 2u);
        if (c == 0) break;
        if (c > 127) return {};
        s.push_back(static_cast<char>(c));
    }
    return s;
}
