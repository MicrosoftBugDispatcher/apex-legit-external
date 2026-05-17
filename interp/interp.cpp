#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include <random>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include "interp.h"
#include "../memory/comms.hpp"
#include "../sdk/sdk.hpp"
#include "../config/config.h"
#include "../aimbot/aimbot.h"

AimState  g_aim_state;
uintptr_t g_aim_lp = 0;

static double g_ou_x = 0.0, g_ou_y = 0.0;
static double g_tremor_phase_x = 0.0, g_tremor_phase_y = 0.0;
static double g_tremor_freq = 10.0;
static std::mt19937_64 g_rng { std::random_device{}() };

void reset_noise_state()
{
    g_ou_x = g_ou_y = 0.0;
    g_tremor_phase_x = std::uniform_real_distribution<double>(0.0, 6.283)(g_rng);
    g_tremor_phase_y = std::uniform_real_distribution<double>(0.0, 6.283)(g_rng);
    g_tremor_freq    = std::uniform_real_distribution<double>(8.0, 12.0)(g_rng);
}

static volatile bool g_interp_run = false;

static DWORD WINAPI InterpThread(LPVOID)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    timeBeginPeriod(1);

    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    std::normal_distribution<double> nd(0.0, 1.0);

    while (g_interp_run) {
        Sleep(1);

        if (!g_aim_state.active) continue;

        uintptr_t lp = g_aim_lp;
        if (!lp) continue;

        QueryPerformanceCounter(&now);
        double dt_s = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
        prev = now;
        if (dt_s <= 0.0 || dt_s > 0.1) dt_s = 0.001;

        float tx = g_aim_state.target_x;
        float ty = g_aim_state.target_y;
        float dist = g_aim_state.target_dist;

        float va_x = Kernel::Object->Read<float>(lp + offsets::viewangles);
        float va_y = Kernel::Object->Read<float>(lp + offsets::viewangles + 0x4);

        float rate = 1.f / (0.01f + g_aim_smooth * 0.08f);

        float dx = NormalizeAngle(tx - va_x);
        float dy = NormalizeAngle(ty - va_y);

        if (sqrtf(dx * dx + dy * dy) < 0.1f) continue;

        float step_x = dx * rate * (float)dt_s;
        float step_y = dy * rate * (float)dt_s;

        double speed = (double)sqrtf(step_x * step_x + step_y * step_y);

        // Scale noise down at long range so aim stays tight on small targets.
        // Full noise up to ~50m (1270 units), then drops proportional to 1/dist.
        constexpr double REF_DIST  = 1270.0;  // ~50 metres in Apex units
        constexpr double MIN_SCALE = 0.08;     // floor so noise never fully vanishes
        double ns = 1.0;
        if ((double)dist > REF_DIST)
            ns = REF_DIST / (double)dist;
        if (ns < MIN_SCALE) ns = MIN_SCALE;

        constexpr double HUMAN_GLOBAL = 0.7;  // ~30% less humanization overall
        double ou_diff = 0.025 * ns * HUMAN_GLOBAL;
        g_ou_x += -4.5 * g_ou_x * dt_s + ou_diff * std::sqrt(dt_s) * nd(g_rng);
        g_ou_y += -4.5 * g_ou_y * dt_s + ou_diff * std::sqrt(dt_s) * nd(g_rng);

        double t_s = now.QuadPart / (double)freq.QuadPart;
        double trem_mod = 1.0 / (1.0 + speed * 275.0);
        double tr_x = 0.012 * ns * HUMAN_GLOBAL * trem_mod * std::sin(2.0 * 3.14159265 * g_tremor_freq * t_s + g_tremor_phase_x);
        double tr_y = 0.012 * ns * HUMAN_GLOBAL * trem_mod * std::sin(2.0 * 3.14159265 * g_tremor_freq * t_s + g_tremor_phase_y);

        double sdn_x = 0.003 * ns * HUMAN_GLOBAL * speed * nd(g_rng);
        double sdn_y = 0.003 * ns * HUMAN_GLOBAL * speed * nd(g_rng);

        float out_x = NormalizeAngle(va_x + step_x + (float)(g_ou_x + tr_x + sdn_x));
        float out_y = NormalizeAngle(va_y + step_y + (float)(g_ou_y + tr_y + sdn_y));
        if (out_x >  89.f) out_x =  89.f;
        if (out_x < -89.f) out_x = -89.f;

        Kernel::Object->Write<float>(lp + offsets::viewangles,       out_x);
        Kernel::Object->Write<float>(lp + offsets::viewangles + 0x4, out_y);
    }

    timeEndPeriod(1);
    return 0;
}

void start_interp_thread()
{
    g_interp_run = true;
    CloseHandle(CreateThread(NULL, 0, InterpThread, NULL, 0, NULL));
}

void stop_interp_thread()
{
    g_interp_run = false;
    Sleep(50);
}
