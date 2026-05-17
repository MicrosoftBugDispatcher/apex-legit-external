#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include "../sdk/sdk.hpp"
#include "../memory/comms.hpp"
#include "../entities/entity.hpp"

namespace glow {

constexpr uint8_t GLOW_THROUGH_WALL = 1;

static_assert(offsets::highlight_type_size == 0x34, "highlight struct size mismatch");

static const std::unordered_set<std::string> g_item_signifiers = {
    "ammo_box",
    "mp_weapon",
    "item_",
    "_ammo",
    "_health",
    "_shield",
    "_armor",
    "_phoenix",
    "_batteries",
    "_nade",
    "_frag",
    "_arc",
    "_thermite",
    "_molotov",
    "survey_beacon",
    "hopup",
    "_optic",
    "__barrel",
    "__mag",
    "__stock",
    "__grip",
    "__shield_cell",
    "_syringe",
    "_backpack",
    "prop_r5_ammo",
    "prop_survival",
    "prop_weapon",
    "prop_weap",
    "_loot_tier",
    "loot_drone",
    "supply_bin",
    "loot_tick",
    "item_shield",
};

static const std::unordered_set<std::string> g_vehicle_signifiers = {
    "mp_vehicle",
    "_vehicle",
    "prop_vehicle",
    "r5_tether_drone",
    "uav_drone",
    "droppod",
    "turret",
};

inline bool has_valid_pointer(uintptr_t addr) {
    return addr && addr > 0x1000;
}

inline void apply_glow(Kernel::Driver* drv, uintptr_t ent, uint8_t highlight_id, float distance = 0.f) {
    if (!has_valid_pointer(ent)) return;

    drv->Write<uint8_t>(ent + offsets::m_highlightVisibilityType,    GLOW_THROUGH_WALL);
    drv->Write<uint8_t>(ent + offsets::m_highlightServerActiveStates, highlight_id);
    drv->Write<float>(ent + offsets::Highlight_SetFarFadeDist,       distance);
}

inline bool is_item_entity(Kernel::Driver* drv, uintptr_t ent) {
    std::string name = ReadEntityName(drv, ent);
    if (name.empty()) return false;

    for (const auto& sig : g_item_signifiers) {
        if (name.find(sig) != std::string::npos)
            return true;
    }
    return false;
}

inline bool is_vehicle_entity(Kernel::Driver* drv, uintptr_t ent) {
    std::string name = ReadEntityName(drv, ent);
    if (name.empty()) return false;

    for (const auto& sig : g_vehicle_signifiers) {
        if (name.find(sig) != std::string::npos)
            return true;
    }
    return false;
}

inline bool is_player_entity(Kernel::Driver* drv, uintptr_t ent) {
    if (!has_valid_pointer(ent)) return false;

    uintptr_t bonePtr = drv->Read<uintptr_t>(ent + offsets::bone_array);
    if (!has_valid_pointer(bonePtr)) return false;

    int health = drv->Read<int>(ent + offsets::m_iHealth);
    return health > 0 && health <= 500;
}

inline int get_team(Kernel::Driver* drv, uintptr_t ent) {
    return drv->Read<int>(ent + offsets::m_iTeamNum);
}

inline uintptr_t get_weapon(Kernel::Driver* drv, uintptr_t lp) {
    return drv->Read<uintptr_t>(lp + 0x1980);
}

enum class EntityType {
    Unknown,
    Player,
    Weapon,
    Item,
    Vehicle,
};

inline EntityType get_entity_type(Kernel::Driver* drv, uintptr_t ent) {
    if (!has_valid_pointer(ent)) return EntityType::Unknown;

    if (is_player_entity(drv, ent))
        return EntityType::Player;

    if (is_vehicle_entity(drv, ent))
        return EntityType::Vehicle;

    if (is_item_entity(drv, ent))
        return EntityType::Item;

    std::string name = ReadEntityName(drv, ent);
    if (!name.empty() && name.find("weapon") != std::string::npos)
        return EntityType::Weapon;

    return EntityType::Unknown;
}

struct GlowConfig {
    bool player_glow    = true;
    bool enemy_glow     = true;
    bool ally_glow      = true;
    bool weapon_glow    = false;
    bool item_glow      = false;
    bool vehicle_glow   = false;
};

}
