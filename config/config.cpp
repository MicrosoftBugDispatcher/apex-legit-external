#include "config.h"

bool     g_player_glow   = true;
bool     g_enemy_glow    = true;
bool     g_ally_glow     = true;
bool     g_weapon_glow   = true;
bool     g_item_glow     = true;
bool     g_vehicle_glow  = true;
uint8_t  g_glow_id       = 47;

bool      g_aimbot_enabled = false;
uintptr_t g_locked_target  = 0;
float     g_aim_smooth     = 0.2f;
float     g_aim_fov        = 6.f;

int       g_max_entities   = 10000;
