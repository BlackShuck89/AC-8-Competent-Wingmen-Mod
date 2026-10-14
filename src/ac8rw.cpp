#include "ac8rw.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>

struct AC8RW_State {
    std::unordered_map<uint64_t, uint64_t> previous_targets;
    std::unordered_map<uint64_t, double> last_launches;
};

namespace {
constexpr double neg_inf = -std::numeric_limits<double>::infinity();
uint32_t role_from_string(const char *name) {
    if (!name) return AC8RW_MULTIROLE;
    std::string aircraft(name);
    for (char &ch : aircraft) ch = (char)std::toupper((unsigned char)ch);
    struct Match { const char *token; uint32_t role; };
    static constexpr Match matches[] = {
        {"EA-18G", 4}, {"E/A-18G", 4}, {"EF-111", 4}, {"EA-6B", 4},
        {"A-10", 2}, {"SU-25", 2}, {"F-117", 2},
        {"F-15E", 3}, {"F/A-18", 3}, {"F-16", 3}, {"RAFALE", 3},
        {"TYPHOON", 3}, {"MIRAGE 2000", 3},
        {"F-22", 1}, {"F-15C", 1}, {"SU-27", 1}, {"MIG-29", 1},
        {"F-14", 1}, {"MIG-31", 1},
    };
    uint32_t best = AC8RW_MULTIROLE;
    size_t length = 0;
    for (const auto &m : matches) {
        size_t size = std::strlen(m.token);
        if (size > length && aircraft.find(m.token) != std::string::npos) {
            length = size; best = m.role;
        }
    }
    return best;
}
const AC8RW_Profile &get_profile(const AC8RW_Config *cfg, uint32_t role) {
    return cfg->profiles[(role >= AC8RW_FIGHTER && role <= AC8RW_ELECTRONIC_WARFARE) ? role : (uint32_t)AC8RW_MULTIROLE];
}
double weight(const AC8RW_Profile &p, uint32_t kind) {
    if (kind == AC8RW_KIND_AIR) return p.air;
    if (kind == AC8RW_KIND_GROUND) return p.ground;
    if (kind == AC8RW_KIND_SURFACE) return p.surface;
    return 0;
}
double score(const AC8RW_Wingman &w, const AC8RW_Target &t, const AC8RW_Profile &p,
             uint32_t role, const std::unordered_map<uint64_t, int> &allocations) {
    if (!(t.flags & AC8RW_TARGET_ALIVE) || (t.flags & AC8RW_TARGET_FRIENDLY) ||
        (t.flags & AC8RW_TARGET_UNTARGETABLE) || t.id == 0) return neg_inf;
    if (t.kind != AC8RW_KIND_AIR && p.supports_ground_strikes == 0) return neg_inf;
    double type = weight(p, t.kind);
    if (!std::isfinite(type) || type <= 0) return neg_inf;
    double distance = t.distance_km;
    if ((w.flags & AC8RW_WING_HAS_POSITION) && (t.flags & AC8RW_TARGET_HAS_POSITION)) {
        double dx = w.x_cm - t.x_cm, dy = w.y_cm - t.y_cm, dz = w.z_cm - t.z_cm;
        distance = std::sqrt(dx * dx + dy * dy + dz * dz) / 100000.;
    }
    if (!std::isfinite(distance) || distance < 0) distance = 30;
    double range_w = std::clamp(1.45 - distance / 80., 0.4, 1.45);
    double importance = (std::isfinite(t.importance) && t.importance > 0) ? t.importance : 1;
    if (t.flags & AC8RW_TARGET_MISSION_CRITICAL) importance *= 1.45;
    if (t.flags & AC8RW_TARGET_SAM) importance *= 1.35;
    if (t.flags & AC8RW_TARGET_BOMBER) importance *= 1.30;
    bool imminent = (t.flags & (AC8RW_TARGET_THREATENS_PLAYER | AC8RW_TARGET_THREATENS_ALLY)) != 0;
    double pressure = imminent ? p.threat : 1.;
    double objective = (t.flags & AC8RW_TARGET_MISSION_CRITICAL) ? p.objective : 1.;
    int already = 0;
    auto a = allocations.find(t.id);
    if (a != allocations.end()) already = a->second;
    double saturation = 1 / (1 + 0.47 * already);
    double survivability = (t.flags & AC8RW_TARGET_SAM) && role != AC8RW_ATTACKER &&
        t.kind != AC8RW_KIND_AIR ? 0.77 : 1.;
    double value = type * importance * range_w * pressure * objective * saturation * survivability;
    if (role == AC8RW_ELECTRONIC_WARFARE) value *= imminent ? 1.9 : 0.38;
    if (w.current_target_id && w.current_target_id == t.id) value *= 1.15;
    return value;
}
uint32_t launch_reason(const AC8RW_Wingman &w, const AC8RW_Target *t, const AC8RW_Profile &p,
    uint32_t role, const AC8RW_State *state, double now) {
    if (!t) return AC8RW_NO_TARGET;
    if (w.missile_ammo == 0) return AC8RW_NO_AMMO;
    if (!(w.flags & AC8RW_WING_LOCK_READY)) return AC8RW_NO_LOCK;
    if (!(w.flags & AC8RW_WING_SAFE_TO_FIRE)) return AC8RW_UNSAFE;
    if (!(w.flags & AC8RW_WING_MISSILE_READY)) return AC8RW_WEAPON_UNAVAILABLE;
    if (!std::isfinite(w.lock_quality)) return AC8RW_UNKNOWN_LOCK_QUALITY;
    if (!std::isfinite(w.hit_chance)) return AC8RW_UNKNOWN_HIT_CHANCE;
    if (w.lock_quality < p.min_lock_quality) return AC8RW_WEAK_LOCK;
    if (w.hit_chance < p.min_hit_chance) return AC8RW_POOR_SOLUTION;
    if (auto a = state->last_launches.find(w.id); a != state->last_launches.end()) {
        if (now < a->second || now - a->second < p.missile_cooldown_s) return AC8RW_COOLDOWN;
    }
    if (role == AC8RW_ELECTRONIC_WARFARE &&
        !(t->flags & (AC8RW_TARGET_THREATENS_PLAYER | AC8RW_TARGET_THREATENS_ALLY))) {
        return AC8RW_EW_RESERVE;
    }
    return AC8RW_FIRE;
}
std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}
std::string lower(std::string s) {
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
}

extern "C" {
uint32_t AC8RW_ApiVersion() { return AC8RW_ABI_VERSION; }
uint32_t AC8RW_InferRole(const char *name) { return role_from_string(name); }
const char *AC8RW_ReasonName(uint32_t reason) {
    static const char *names[] = {"fire", "no_target", "no_ammo", "no_lock", "unsafe",
        "weapon_unavailable", "unknown_lock_quality", "unknown_hit_chance", "weak_lock",
        "poor_solution", "cooldown", "ew_reserve", "invalid_input"};
    return (reason <= AC8RW_INVALID_INPUT) ? names[reason] : "unknown";
}
void AC8RW_DefaultConfig(AC8RW_Config *out) {
    if (!out) return;
    std::memset(out, 0, sizeof(*out));
    out->abi_version = AC8RW_ABI_VERSION;
    out->target_switch_advantage = 1.35;
    out->saturation_factor = 0.47;
    out->profiles[1] = {1.65, 0.25, 0.35, 1.35, 1.10, 3.0, 0.55, 0.40, 0};
    out->profiles[2] = {0.30, 1.65, 1.50, 1.15, 1.35, 4.3, 0.60, 0.48, 1};
    out->profiles[3] = {1.12, 1.00, 1.00, 1.20, 1.20, 3.5, 0.54, 0.43, 1};
    out->profiles[4] = {0.90, 0.45, 0.50, 1.70, 0.60, 5.0, 0.67, 0.53, 0};
}
int AC8RW_LoadConfigIni(AC8RW_Config *config, const char *filename) {
    if (!config || !filename || config->abi_version != AC8RW_ABI_VERSION) return 0;
    std::ifstream file(filename);
    if (!file) return 0;
    std::string section, line;
    while (std::getline(file, line)) {
        auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) line.resize(comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line.front() == '[' && line.back() == ']') { section = lower(trim(line.substr(1, line.size()-2))); continue; }
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = lower(trim(line.substr(0, equals)));
        const std::string textvalue = trim(line.substr(equals + 1));
        double val;
        try {
            size_t consumed = 0;
            val = std::stod(textvalue, &consumed);
            if (!std::isfinite(val) || consumed != textvalue.size()) continue;
        } catch (...) { continue; }
        if (section == "general") {
            if (key == "target_switch_advantage" && val >= 1 && val <= 5) config->target_switch_advantage = val;
            if (key == "saturation_factor" && val >= 0 && val <= 5) config->saturation_factor = val;
            continue;
        }
        uint32_t role = section == "fighter" ? 1 : section == "attacker" ? 2 :
            section == "multirole" ? 3 : section == "electronic_warfare" ? 4 : 0;
        if (!role) continue;
        auto &p = config->profiles[role];
        if ((key == "air" || key == "ground" || key == "surface" || key == "threat" ||
             key == "objective") && val >= 0 && val <= 10) {
            if (key == "air") p.air = val;
            if (key == "ground") p.ground = val;
            if (key == "surface") p.surface = val;
            if (key == "threat") p.threat = val;
            if (key == "objective") p.objective = val;
        } else if (key == "missile_cooldown_s" && val >= 0 && val <= 120) p.missile_cooldown_s = val;
        else if (key == "min_lock_quality" && val >= 0 && val <= 1) p.min_lock_quality = val;
        else if (key == "min_hit_chance" && val >= 0 && val <= 1) p.min_hit_chance = val;
        else if (key == "supports_ground_strikes" && (val == 0 || val == 1)) p.supports_ground_strikes = (uint32_t)val;
    }
    return 1;
}
AC8RW_State *AC8RW_Create() { return new AC8RW_State; }
void AC8RW_Destroy(AC8RW_State *p) { delete p; }
void AC8RW_ConfirmMissileLaunch(AC8RW_State *s, uint64_t wingman_id, double now) {
    if (s && wingman_id && std::isfinite(now)) s->last_launches[wingman_id] = now;
}
int AC8RW_Plan(AC8RW_State *state, const AC8RW_Config *cfg,
    const AC8RW_Wingman *wings, size_t n_wings, const AC8RW_Target *targets, size_t n_targets,
    double now, AC8RW_Command *out, size_t capacity) {
    if (!state || !cfg || cfg->abi_version != AC8RW_ABI_VERSION || !std::isfinite(now) ||
        (n_wings && !wings) || (n_targets && !targets) || (n_wings && !out) ||
        capacity < n_wings || n_wings > 256 || n_targets > 10000) return -1;
    std::unordered_map<uint64_t, int> assignments;
    for (size_t i = 0; i < n_wings; ++i) {
        const AC8RW_Wingman &w = wings[i];
        uint32_t role = w.role_override ? w.role_override : AC8RW_InferRole(w.aircraft_name);
        if (role < 1 || role > 4) role = AC8RW_MULTIROLE;
        const AC8RW_Profile &p = get_profile(cfg, role);
        const AC8RW_Target *best = nullptr;
        double best_score = neg_inf;
        auto calculate = [&](const AC8RW_Target &t) {
            /* Saturation factor is configurable, so adjust compared to the default score function. */
            double value = score(w, t, p, role, assignments);
            auto a = assignments.find(t.id);
            if (a != assignments.end() && std::isfinite(value)) {
                value *= (1 + 0.47*a->second) / (1 + cfg->saturation_factor*a->second);
            }
            return value;
        };
        for (size_t j = 0; j < n_targets; ++j) {
            double s = calculate(targets[j]);
            if (s > best_score) { best_score = s; best = &targets[j]; }
        }
        uint64_t old_id = 0;
        auto old = state->previous_targets.find(w.id);
        if (old != state->previous_targets.end()) old_id = old->second;
        if (old_id && best && old_id != best->id) {
            for (size_t j = 0; j < n_targets; ++j) {
                if (targets[j].id == old_id) {
                    double old_score = calculate(targets[j]);
                    if (std::isfinite(old_score) && best_score < old_score * cfg->target_switch_advantage) {
                        best = &targets[j]; best_score = old_score;
                    }
                    break;
                }
            }
        }
        if (!std::isfinite(best_score)) best = nullptr;
        uint32_t reason = launch_reason(w, best, p, role, state, now);
        out[i] = {w.id, best ? best->id : 0, role,
            role == AC8RW_FIGHTER ? AC8RW_INTERCEPT : role == AC8RW_ATTACKER ? AC8RW_STRIKE :
            role == AC8RW_ELECTRONIC_WARFARE ? AC8RW_PROTECT : AC8RW_FLEX,
            reason, best ? 1u : 0u, reason == AC8RW_FIRE ? 1u : 0u, best_score};
        if (best) {
            state->previous_targets[w.id] = best->id;
            ++assignments[best->id];
        } else state->previous_targets.erase(w.id);
    }
    return (int)n_wings;
}
}
