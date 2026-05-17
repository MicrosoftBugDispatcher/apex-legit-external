#pragma once
#include <cstdint>
#include "../sdk/sdk.hpp"

float NormalizeAngle(float a);
Vector CalcAngle(Vector src, Vector dst);
Vector GetClosestTargetBone(Kernel::Driver* drv, uintptr_t localPlayer, uintptr_t& outEntity, int screenW, int screenH);
