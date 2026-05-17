#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <vector>
#include "memory/comms.hpp"
#include "sdk/sdk.hpp"
#include "entities/entity.hpp"
#include "visuals/glow.hpp"
#include "aimbot/aimbot.h"
#include "interp/interp.h"
#include "config/config.h"

struct CachedEntity {
    uintptr_t addr;
    glow::EntityType type;
};

static std::vector<CachedEntity> g_entity_cache;
static DWORD g_last_cache_tick  = 0;
static const DWORD CACHE_INTERVAL_MS = 2000;

static void refresh_entity_cache(Kernel::Driver* drv, uintptr_t lp)
{
    g_entity_cache.clear();
    g_entity_cache.reserve(g_max_entities);

    for (int i = 0; i < g_max_entities; i++) {
        uintptr_t ent = GetEntityPtr(drv, i);
        if (!glow::has_valid_pointer(ent) || ent == lp)
            continue;

        glow::EntityType type = glow::get_entity_type(drv, ent);
        if (type == glow::EntityType::Unknown)
            continue;

        g_entity_cache.push_back({ ent, type });
    }

    g_last_cache_tick = GetTickCount();
}

static bool init_driver()
{
    HANDLE h = CreateFileW(
        L"\\\\.\\{fat-bunny-device}",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);

    if (!h || h == INVALID_HANDLE_VALUE) {
        std::cout << "[-] driver not found" << std::endl;
        return false;
    }
    CloseHandle(h);
    return true;
}

static bool attach_process()
{
    try {
        Kernel::Object = new Kernel::Driver("r5apex_dx12.exe");
    }
    catch (...) {
        std::cout << "[-] failed to attach to process" << std::endl;
        return false;
    }
    std::cout << "[+] attached, base = 0x"
              << std::hex << Kernel::Object->Base << std::dec << std::endl;
    return true;
}

static void run_glow(Kernel::Driver* drv, uintptr_t lp)
{
    if (g_weapon_glow) {
        uintptr_t weapon = glow::get_weapon(drv, lp);
        if (glow::has_valid_pointer(weapon))
            glow::apply_glow(drv, weapon, g_glow_id);
    }

    int localTeam = glow::get_team(drv, lp);

    for (const auto& ce : g_entity_cache) {
        if (ce.addr == lp) continue;

        int health = drv->Read<int>(ce.addr + offsets::m_iHealth);
        if (health <= 0) continue;

        if (ce.type == glow::EntityType::Player) {
            if (!g_player_glow) continue;
            int entTeam = glow::get_team(drv, ce.addr);
            if (entTeam == localTeam) {
                if (g_ally_glow) glow::apply_glow(drv, ce.addr, g_glow_id);
            } else {
                if (g_enemy_glow) glow::apply_glow(drv, ce.addr, g_glow_id);
            }
        }
        else if (ce.type == glow::EntityType::Weapon && g_weapon_glow) {
            glow::apply_glow(drv, ce.addr, g_glow_id);
        }
        else if (ce.type == glow::EntityType::Item && g_item_glow) {
            glow::apply_glow(drv, ce.addr, g_glow_id);
        }
        else if (ce.type == glow::EntityType::Vehicle && g_vehicle_glow) {
            glow::apply_glow(drv, ce.addr, g_glow_id);
        }
    }
}

static void run_aimbot(Kernel::Driver* drv, uintptr_t lp, int screenW, int screenH)
{
    Vector cam = GetCameraPosition(drv, lp);
    Vector targetBone = GetClosestTargetBone(drv, lp, g_locked_target, screenW, screenH);

    if (targetBone.x != 0 || targetBone.y != 0 || targetBone.z != 0) {
        Vector desired = CalcAngle(cam, targetBone);
        float ddx = targetBone.x - cam.x;
        float ddy = targetBone.y - cam.y;
        float ddz = targetBone.z - cam.z;
        g_aim_state.target_x    = desired.x;
        g_aim_state.target_y    = desired.y;
        g_aim_state.target_dist = sqrtf(ddx * ddx + ddy * ddy + ddz * ddz);
        g_aim_state.active      = true;
    } else {
        g_aim_state.active = false;
    }
}

int main()
{
    if (!init_driver())    return -1;
    if (!attach_process()) return -1;

    Kernel::Driver* drv = Kernel::Object;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    start_interp_thread();

    while (true) {
        g_aimbot_enabled = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        uintptr_t lp = drv->Read<uintptr_t>(drv->Base + offsets::local_player);
        if (!lp || lp <= 0x1000) {
            g_aim_lp = 0;
            g_aim_state.active = false;
            g_entity_cache.clear();
            Sleep(1);
            continue;
        }

        uintptr_t lpBoneArray = drv->Read<uintptr_t>(lp + offsets::bone_array);
        if (!lpBoneArray || lpBoneArray <= 0x1000) {
            Sleep(1);
            continue;
        }

        g_aim_lp = lp;
        UpdateViewMatrix(drv);

        DWORD now = GetTickCount();
        if (g_entity_cache.empty() || (now - g_last_cache_tick) >= CACHE_INTERVAL_MS)
            refresh_entity_cache(drv, lp);

        if (!g_aimbot_enabled) {
            if (g_locked_target != 0 || g_aim_state.active) {
                g_locked_target = 0;
                g_aim_state.active = false;
                reset_noise_state();
            }
        }

        if (g_aimbot_enabled)
            run_aimbot(drv, lp, screenW, screenH);

        run_glow(drv, lp);

        Sleep(1);
    }

    stop_interp_thread();
    delete drv;
    return 0;
}
