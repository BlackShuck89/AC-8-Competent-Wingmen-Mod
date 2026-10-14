/*
 * ACE COMBAT 8 Role-Aware Wingmen — portable native tactical-planner ABI.
 * Copyright (c) 2026 — project authors. MIT license; see LICENSE.
 * No UE4SS dependency and no Unreal Engine headers are required.
 * IMPORTANT: This is not an in-game hook, mod loader, or anti-cheat bypass.
 */
#ifndef AC8RW_H
#define AC8RW_H
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(AC8RW_BUILD)
#    define AC8RW_API __declspec(dllexport)
#  else
#    define AC8RW_API __declspec(dllimport)
#  endif
#else
#  define AC8RW_API __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif

enum { AC8RW_ABI_VERSION = 2 };
typedef enum AC8RW_Role {
    AC8RW_ROLE_AUTO = 0, AC8RW_FIGHTER = 1, AC8RW_ATTACKER = 2,
    AC8RW_MULTIROLE = 3, AC8RW_ELECTRONIC_WARFARE = 4
} AC8RW_Role;
typedef enum AC8RW_Kind {
    AC8RW_KIND_AIR = 1, AC8RW_KIND_GROUND = 2, AC8RW_KIND_SURFACE = 3
} AC8RW_Kind;
typedef enum AC8RW_Posture {
    AC8RW_INTERCEPT = 1, AC8RW_STRIKE = 2, AC8RW_FLEX = 3, AC8RW_PROTECT = 4
} AC8RW_Posture;
typedef enum AC8RW_Reason {
    AC8RW_FIRE = 0, AC8RW_NO_TARGET, AC8RW_NO_AMMO, AC8RW_NO_LOCK,
    AC8RW_UNSAFE, AC8RW_WEAPON_UNAVAILABLE, AC8RW_UNKNOWN_LOCK_QUALITY,
    AC8RW_UNKNOWN_HIT_CHANCE, AC8RW_WEAK_LOCK, AC8RW_POOR_SOLUTION,
    AC8RW_COOLDOWN, AC8RW_EW_RESERVE, AC8RW_INVALID_INPUT
} AC8RW_Reason;

enum AC8RW_WingFlags {
    AC8RW_WING_HAS_POSITION = 1 << 0, AC8RW_WING_LOCK_READY = 1 << 1,
    AC8RW_WING_MISSILE_READY = 1 << 2, AC8RW_WING_SAFE_TO_FIRE = 1 << 3
};
enum AC8RW_TargetFlags {
    AC8RW_TARGET_ALIVE = 1 << 0, AC8RW_TARGET_FRIENDLY = 1 << 1,
    AC8RW_TARGET_UNTARGETABLE = 1 << 2, AC8RW_TARGET_THREATENS_PLAYER = 1 << 3,
    AC8RW_TARGET_THREATENS_ALLY = 1 << 4, AC8RW_TARGET_MISSION_CRITICAL = 1 << 5,
    AC8RW_TARGET_SAM = 1 << 6, AC8RW_TARGET_BOMBER = 1 << 7,
    AC8RW_TARGET_HAS_POSITION = 1 << 8
};

typedef struct AC8RW_Profile {
    double air, ground, surface;
    double threat, objective;
    double missile_cooldown_s, min_lock_quality, min_hit_chance;
    uint32_t supports_ground_strikes;
} AC8RW_Profile;
typedef struct AC8RW_Config {
    uint32_t abi_version;
    double target_switch_advantage;
    double saturation_factor;
    AC8RW_Profile profiles[5]; /* index = AC8RW_Role; [0] unused */
} AC8RW_Config;

typedef struct AC8RW_Wingman {
    uint64_t id;
    const char *aircraft_name; /* valid for duration of the call */
    uint32_t role_override;    /* AC8RW_ROLE_AUTO = infer from name */
    uint32_t flags;
    uint32_t missile_ammo;
    uint64_t current_target_id;
    double x_cm, y_cm, z_cm;   /* game UE units, centimeters */
    double lock_quality;       /* 0.0...1.0 or NAN if unknown */
    double hit_chance;         /* 0.0...1.0 or NAN if unknown */
} AC8RW_Wingman;

typedef struct AC8RW_Target {
    uint64_t id;
    uint32_t kind;  /* AC8RW_Kind */
    uint32_t flags;
    double x_cm, y_cm, z_cm;
    double distance_km; /* nonnegative fallback if positions aren't available */
    double importance;  /* > 0, 1.0 standard */
} AC8RW_Target;

typedef struct AC8RW_Command {
    uint64_t wingman_id;
    uint64_t target_id; /* 0 if none */
    uint32_t role;
    uint32_t posture;
    uint32_t reason;
    uint32_t has_target;
    uint32_t suggest_missile_launch; /* suggestion only: never fires weapons */
    double target_score;
} AC8RW_Command;

typedef struct AC8RW_State AC8RW_State;
AC8RW_API uint32_t AC8RW_ApiVersion(void);
AC8RW_API void AC8RW_DefaultConfig(AC8RW_Config *out);
/* Config file may override profile numeric values. Returns 1 on success, 0 on failure. */
AC8RW_API int AC8RW_LoadConfigIni(AC8RW_Config *config, const char *filename);
AC8RW_API AC8RW_State *AC8RW_Create(void);
AC8RW_API void AC8RW_Destroy(AC8RW_State *state);
/*
 * Returns number of commands written (up to out_capacity), or -1 on invalid input.
 * Caller MUST supply only verified friendly AI wingmen and hostile target candidates;
 * the planner has no access to AC8 UObject reflection/factions by itself.
 * This method does not call game functions or fire missiles.
 */
AC8RW_API int AC8RW_Plan(AC8RW_State *state, const AC8RW_Config *config,
    const AC8RW_Wingman *wingmen, size_t wingman_count,
    const AC8RW_Target *targets, size_t target_count,
    double mission_time_s, AC8RW_Command *out, size_t out_capacity);
/* Only notify the planner AFTER the game actually confirmed a launched missile. */
AC8RW_API void AC8RW_ConfirmMissileLaunch(AC8RW_State *state,
    uint64_t wingman_id, double mission_time_s);
AC8RW_API uint32_t AC8RW_InferRole(const char *aircraft_name);
AC8RW_API const char *AC8RW_ReasonName(uint32_t reason);
#ifdef __cplusplus
}
#endif
#endif
