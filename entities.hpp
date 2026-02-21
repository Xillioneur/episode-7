#pragma once
#include "types.hpp"
#include <vector>
#include <string>
#include <ranges>
#include <memory>
#include <expected>
#include <set>

enum class ObjType { PLAYER, GLITCH_BASIC, GLITCH_DASH, GLITCH_TANK, GLITCH_BOSS, GLITCH_PROJECTILE, GLITCH_SPLITTER, HARMONY_RAY, HARMONY_ORB, FRUIT_OF_SPIRIT, PARTICLE, HARMONY_PULSE };

class GameObject {
public:
    Vec2 pos, prev_pos, vel;
    float radius = 10.0f;
    bool ascended = false;
    ObjType type;
    Color color = Colors::WHITE;
    float angle = 0.0f;
    
    GameObject(Vec2 p, ObjType t) : pos(p), prev_pos(p), type(t) {}
    virtual ~GameObject() = default;
    virtual void update(float dt) {
        prev_pos = pos;
        pos = pos + vel * dt;
    }
};

class GlitchProjectile : public GameObject {
public:
    float life = 3.0f;
    GlitchProjectile(Vec2 p, Vec2 v) : GameObject(p, ObjType::GLITCH_PROJECTILE) {
        vel = v;
        radius = 5.0f;
        color = Colors::RED; 
    }
    void update(float dt) override {
        GameObject::update(dt);
        life -= dt;
        if(life <= 0) ascended = true;
    }
};

struct BackgroundStar {
    Vec2 pos;
    float depth; 
    float size;
    Color color;
};

// Optimized Particle for the Chaos Pool
struct ParticleData {
    Vec2 pos, vel;
    Color color;
    float life = 0.0f;
    float decay = 1.0f;
    float radius = 2.0f;
    bool active = false;
};

class HarmonyRay : public GameObject {
public:
    float damage = 10.0f;
    float life = 0.5f; 
    int pierce_left = 0;
    std::set<uintptr_t> hits; // Track unique glitch IDs hit to prevent double-damage

    HarmonyRay(Vec2 p, Vec2 v, float dmg, Color c, int pierce) 
        : GameObject(p, ObjType::HARMONY_RAY), pierce_left(pierce) {
        vel = v;
        damage = dmg;
        color = c;
        radius = 2.0f;
    }
    void update(float dt) override {
        prev_pos = pos;
        GameObject::update(dt);
        life -= dt;
        if(life <= 0) ascended = true;
    }
};

class HarmonyOrb : public GameObject {
public:
    float value;
    float pulse_timer = 0.0f;
    HarmonyOrb(Vec2 p, float val) : GameObject(p, ObjType::HARMONY_ORB), value(val) {
        vel = Vec2(rnd(-30, 30), rnd(-30, 30));
        radius = 8.0f;
        color = Colors::HARMONY_GOLD;
        pulse_timer = rnd(0, 6.28f);
    }
    void update(float dt) override {
        pulse_timer += dt * 5.0f;
        pos = pos + vel * dt;
        vel = vel * 0.92f;
    }
};

class HarmonyPulse : public GameObject {
public:
    float max_r;
    float current_r = 0.0f;
    float speed = 800.0f;
    HarmonyPulse(Vec2 p, float r, Color c) : GameObject(p, ObjType::HARMONY_PULSE), max_r(r) {
        color = c;
    }
    void update(float dt) override {
        current_r += speed * dt;
        if (current_r >= max_r) ascended = true;
    }
};

class FruitOfSpirit : public GameObject {
public:
    std::string virtue;
    float life = 10.0f;
    FruitOfSpirit(Vec2 p, const std::string& v, Color c) : GameObject(p, ObjType::FRUIT_OF_SPIRIT), virtue(v) {
        color = c;
        radius = 12.0f;
        vel = Vec2(0, 40.0f); // Descends from above
    }
    void update(float dt) override {
        pos = pos + vel * dt;
        life -= dt;
        if(life <= 0) ascended = true;
    }
};

class Player : public GameObject {
public:
    float speed = 400.0f;
    float max_integrity = 100.0f;
    float integrity = 100.0f;
    float harmony_rate = 0.12f;
    float harmony_timer = 0.0f;
    float dash_cooldown = 0.0f;
    int level = 1;
    float harmony_xp = 0;
    float harmony_next = 50;
    float internal_pulse = 1.0f;
    float siphon_range = 250.0f;
    float regen_scale = 0.0f;
    
    // Blessings
    float blessing_timer = 0.0f;
    std::string active_virtue = "";
    
    struct TrailPoint { Vec2 pos; float angle; float life; };
    std::vector<TrailPoint> trail;
    
    // Evolutions
    int ray_count = 1;
    float ray_spread = 0.12f;
    int ray_pierce = 0;
    float ray_speed = 1800.0f; 
    float harmony_power = 15.0f;

    Player(Vec2 p) : GameObject(p, ObjType::PLAYER) {
        radius = 12.0f;
        color = Colors::NEXUS_WHITE;
    }

    void update(float dt) override {
        pos = pos + vel * dt;
        vel = vel * 0.92f;
        if (harmony_timer > 0) harmony_timer -= dt;
        if (dash_cooldown > 0) dash_cooldown -= dt;

        // Record trail
        static float trail_timer = 0;
        trail_timer += dt;
        if (trail_timer > 0.03f) {
            trail.push_back({pos, angle, 1.0f});
            trail_timer = 0;
        }
        for (auto& tp : trail) tp.life -= dt * 2.5f;
        std::erase_if(trail, [](const auto& tp) { return tp.life <= 0; });
    }
};

class Glitch : public GameObject {
public:
    float stability;
    float max_stability;
    std::vector<Vec3> vertices;
    std::vector<std::pair<int, int>> edges;
    float rx = 0, ry = 0, rz = 0;
    float rvx, rvy, rvz;
    float hit_timer = 0.0f;
    int internal_state = 0;
    float state_timer = 0.0f;
    float aura_radius = 0.0f;

    Glitch(Vec2 p, ObjType t, float difficulty_mult) : GameObject(p, t) {
        float scale = 1.0f + difficulty_mult * 0.12f;
        rvx = rnd(-2, 2); rvy = rnd(-2, 2); rvz = rnd(-2, 2);

        if (t == ObjType::GLITCH_BASIC) {
            max_stability = stability = 60.0f * scale;
            radius = 14.0f;
            color = Colors::VOID_PURPLE;
            // Tetrahedron
            vertices = { {0,-1,0}, {1,1,-1}, {-1,1,-1}, {0,1,1} };
            edges = { {0,1}, {0,2}, {0,3}, {1,2}, {2,3}, {3,1} };
            state_timer = rnd(0.5f, 2.0f);
        } else if (t == ObjType::GLITCH_TANK) {
            max_stability = stability = 100.0f * scale;
            radius = 24.0f;
            color = Colors::SHADOW_DARK;
            aura_radius = 150.0f;
            // Cube
            vertices = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1}, {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1} };
            edges = { {0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7} };
        } else if (t == ObjType::GLITCH_DASH) {
            max_stability = stability = 20.0f * scale;
            radius = 10.0f;
            color = Colors::VOID_VIOLET; 
            // Diamond (Double Pyramid)
            vertices = { {0,-1.5,0}, {0,1.5,0}, {1,0,1}, {1,0,-1}, {-1,0,-1}, {-1,0,1} };
            edges = { {0,2}, {0,3}, {0,4}, {0,5}, {1,2}, {1,3}, {1,4}, {1,5}, {2,3}, {3,4}, {4,5}, {5,2} };
        } else if (t == ObjType::GLITCH_BOSS) {
            max_stability = stability = 1000.0f * (1.0f + difficulty_mult * 0.5f);
            radius = 60.0f;
            color = Colors::RED; 
            // Nested Octahedron (Core + Shell)
            vertices = { 
                {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, // Outer
                {0.5,0,0}, {-0.5,0,0}, {0,0.5,0}, {0,-0.5,0}, {0,0,0.5}, {0,0,-0.5} // Inner
            };
            edges = { 
                {0,2}, {0,3}, {0,4}, {0,5}, {1,2}, {1,3}, {1,4}, {1,5}, {2,4}, {4,3}, {3,5}, {5,2}, // Outer Shell
                {6,8}, {6,9}, {6,10}, {6,11}, {7,8}, {7,9}, {7,10}, {7,11}, {8,10}, {10,9}, {9,11}, {11,8} // Inner Core
            };
        } else if (t == ObjType::GLITCH_SPLITTER) {
            max_stability = stability = 45.0f * scale;
            radius = 18.0f;
            color = Colors::RADIANT_TEAL; 
            // Pyramid-In-Pyramid
            vertices = { {0,-1,0}, {1,1,-1}, {-1,1,-1}, {0,1,1}, {0,-0.5,0}, {0.5,0.5,-0.5}, {-0.5,0.5,-0.5}, {0,0.5,0.5} };
            edges = { {0,1}, {0,2}, {0,3}, {1,2}, {2,3}, {3,1}, {4,5}, {4,6}, {4,7}, {5,6}, {6,7}, {7,5} };
        }
    }

    void update(float dt) override {
        GameObject::update(dt);
        rx += rvx * dt; ry += rvy * dt; rz += rvz * dt;
        if (hit_timer > 0) hit_timer -= dt;
        if (state_timer > 0) state_timer -= dt;
    }
};

inline std::expected<std::unique_ptr<Glitch>, std::string> create_glitch(Vec2 p, ObjType t, float diff) {
    if (t != ObjType::GLITCH_BASIC && t != ObjType::GLITCH_DASH && t != ObjType::GLITCH_TANK && t != ObjType::GLITCH_BOSS && t != ObjType::GLITCH_SPLITTER) {
        return std::unexpected("Invalid glitch type");
    }
    return std::make_unique<Glitch>(p, t, diff);
}