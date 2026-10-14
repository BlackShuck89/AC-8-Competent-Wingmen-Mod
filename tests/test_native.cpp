#include "ac8rw.h"
// Keep these runtime checks active in both Debug and Release builds.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

static AC8RW_Wingman wing(uint64_t id, const char *name, uint32_t role = AC8RW_ROLE_AUTO) {
    return {id, name, role, AC8RW_WING_LOCK_READY | AC8RW_WING_MISSILE_READY | AC8RW_WING_SAFE_TO_FIRE,
        8, 0, 0, 0, 0, .82, .75};
}
static AC8RW_Target target(uint64_t id, uint32_t kind, uint32_t flags, double km) {
    return {id, kind, flags | AC8RW_TARGET_ALIVE, 0,0,0, km,1};
}
int main() {
    AC8RW_Config cfg; AC8RW_DefaultConfig(&cfg);
    assert(AC8RW_ApiVersion() == AC8RW_ABI_VERSION);
    assert(AC8RW_InferRole("EA-18G Growler") == AC8RW_ELECTRONIC_WARFARE);
    assert(AC8RW_InferRole("F-22 Raptor") == AC8RW_FIGHTER);
    assert(AC8RW_InferRole("A-10C") == AC8RW_ATTACKER);
    assert(AC8RW_InferRole("F-15E Strike Eagle") == AC8RW_MULTIROLE);
    auto s = AC8RW_Create();
    AC8RW_Wingman wings[] = {
        wing(100, "F-22"), wing(101, "A-10C"), wing(102, "EA-18G"), wing(103, "F/A-18")
    };
    AC8RW_Target targets[] = {
        target(1, AC8RW_KIND_AIR, AC8RW_TARGET_BOMBER | AC8RW_TARGET_THREATENS_PLAYER, 15),
        target(2, AC8RW_KIND_GROUND, AC8RW_TARGET_MISSION_CRITICAL, 8),
        target(3, AC8RW_KIND_GROUND, AC8RW_TARGET_SAM | AC8RW_TARGET_THREATENS_ALLY, 8),
    };
    AC8RW_Command commands[4];
    assert(AC8RW_Plan(s, &cfg, wings, 4, targets, 3, 100., commands, 4) == 4);
    assert(commands[0].target_id == 1 && commands[0].suggest_missile_launch);
    assert(commands[1].target_id == 2 || commands[1].target_id == 3);
    assert(commands[2].role == AC8RW_ELECTRONIC_WARFARE && commands[2].posture == AC8RW_PROTECT);
    AC8RW_ConfirmMissileLaunch(s, 100, 100.);
    assert(AC8RW_Plan(s, &cfg, wings, 4, targets, 3, 101., commands, 4) == 4);
    assert(commands[0].reason == AC8RW_COOLDOWN);
    assert(AC8RW_Plan(s, &cfg, wings, 4, targets, 3, 104., commands, 4) == 4);
    assert(commands[0].suggest_missile_launch);
    wings[0].flags &= ~AC8RW_WING_LOCK_READY;
    AC8RW_Plan(s, &cfg, wings, 4, targets, 3, 104., commands, 4);
    assert(commands[0].reason == AC8RW_NO_LOCK);
    wings[0].flags |= AC8RW_WING_LOCK_READY;
    wings[0].hit_chance = std::numeric_limits<double>::quiet_NaN();
    AC8RW_Plan(s, &cfg, wings, 4, targets, 3, 104., commands, 4);
    assert(commands[0].reason == AC8RW_UNKNOWN_HIT_CHANCE);
    AC8RW_Target only_ground[] = { target(7, AC8RW_KIND_GROUND, 0, 4) };
    AC8RW_Plan(s, &cfg, wings, 1, only_ground, 1, 105., commands, 4);
    assert(commands[0].target_id == 0);
    AC8RW_Wingman ew = wing(123, "EA-18G");
    AC8RW_Target harmless = target(8, AC8RW_KIND_AIR, 0, 5);
    AC8RW_Plan(s, &cfg, &ew, 1, &harmless, 1, 105., commands, 4);
    assert(commands[0].reason == AC8RW_EW_RESERVE);
    assert(AC8RW_Plan(nullptr, &cfg, wings, 4, targets, 3, 100, commands, 4) == -1);
    AC8RW_Destroy(s);
    std::puts("PASS: native roles, priorities, safe missiles, locks, EW reserve, cooldown, ABI");
    return 0;
}
