#pragma once
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct Vector {
    float x = 0, y = 0, z = 0;
    Vector() = default;
    Vector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct matrix3x4_t {
    float m_flMatVal[3][4];
};

namespace offsets
{
	constexpr uintptr_t m_nForceBone = 0xdb8;
	constexpr uintptr_t viewrender = 0x3d9a008;
	constexpr uintptr_t viewmatrix = 0x11a350;
	constexpr uintptr_t bone_array = (m_nForceBone + 0x48);
	constexpr uintptr_t local_player = 0x2691b88;
	constexpr uintptr_t entity_list = 0x6268be8;
	constexpr uintptr_t m_iTeamNum = 0x334;
	constexpr uintptr_t m_vecAbsVelocity = 0x170;
	constexpr uintptr_t m_vecAbsOrigin = 0x17c;
	constexpr uintptr_t m_iHealth = 0x324;
	constexpr uintptr_t namelist = 0x8c65160;
	constexpr uintptr_t camera_origin = 0x1fd4;
	constexpr uintptr_t m_iName = 0x479;
	constexpr uintptr_t m_iMaxHealth = 0x468;
	constexpr uintptr_t m_iSignifierName = 0x470;
	constexpr uintptr_t name_index = 0x584;
	constexpr uintptr_t studiohdr = 0x1000;
	constexpr uintptr_t m_highlightVisibilityType = 0x26C;
	constexpr uintptr_t m_highlightServerActiveStates = 0x298;
	constexpr uintptr_t Highlight_SetFarFadeDist = 0x294;
	constexpr uintptr_t highlight_type_size = 0x34;
	constexpr uintptr_t viewangles = 0x2610;
}

enum class HitboxType {
	Head = 0, Neck = 1, UpperChest = 2, LowerChest = 3,
	Stomach = 4, Hip = 5, Leftshoulder = 6, Leftelbow = 7,
	Lefthand = 8, Rightshoulder = 9, RightelbowBone = 10,
	Righthand = 11, LeftThighs = 12, Leftknees = 13,
	Leftleg = 14, RightThighs = 16, Rightknees = 17, Rightleg = 18,
};

namespace Kernel { class Driver; }

inline Vector (&s_view_matrix())[4] {
    static Vector m_matrix[4] = {};
    return m_matrix;
}

inline void UpdateViewMatrix(Kernel::Driver* drv) {
    auto& m_matrix = s_view_matrix();
    uintptr_t viewrender_ptr = drv->Read<uintptr_t>(drv->Base + offsets::viewrender);
    if (!viewrender_ptr || viewrender_ptr <= 0x1000) return;
    m_matrix[0] = drv->Read<Vector>(viewrender_ptr + offsets::viewmatrix + 0x0);
    m_matrix[1] = drv->Read<Vector>(viewrender_ptr + offsets::viewmatrix + 0x10);
    m_matrix[2] = drv->Read<Vector>(viewrender_ptr + offsets::viewmatrix + 0x20);
    m_matrix[3] = drv->Read<Vector>(viewrender_ptr + offsets::viewmatrix + 0x30);
}

inline bool WorldToScreen(Vector origin, int screenW, int screenH, Vector& out) {
    auto& m_matrix = s_view_matrix();
    float w = m_matrix[3].x * origin.x + m_matrix[3].y * origin.y + m_matrix[3].z * origin.z + m_matrix[3].z;
    if (w < 0.001f) return false;
    float x = screenW / 2.0f;
    float y = screenH / 2.0f;
    float flMatrix[3][4] = {
        { m_matrix[0].x, m_matrix[0].y, m_matrix[0].z, 0.0f },
        { m_matrix[1].x, m_matrix[1].y, m_matrix[1].z, 0.0f },
        { m_matrix[2].x, m_matrix[2].y, m_matrix[2].z, 0.0f },
    };
    out.x = x + (0.5f * (1.0f + (flMatrix[0][0] * origin.x + flMatrix[0][1] * origin.y + flMatrix[0][2] * origin.z)) * w);
    out.y = y - (0.5f * (1.0f - (flMatrix[1][0] * origin.x + flMatrix[1][1] * origin.y + flMatrix[1][2] * origin.z)) * w);
    return true;
}
