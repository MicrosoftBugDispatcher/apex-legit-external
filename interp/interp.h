#pragma once
#define NOMINMAX
#include <Windows.h>
#include <cstdint>

struct AimState {
    float target_x  = 0.f;
    float target_y  = 0.f;
    float target_dist = 0.f;   // 3D distance to target (game units)
    bool  active     = false;
};

extern AimState  g_aim_state;
extern uintptr_t g_aim_lp;

void reset_noise_state();
void start_interp_thread();
void stop_interp_thread();
