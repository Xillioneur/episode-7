#pragma once
#include "types.hpp"
#include <vector>
#include <string>
#include <ranges>
#include <memory>
#include <expected>

enum class ObjType { PLAYER, GLITCH_BASIC, GLITCH_DASH, GLITCH_TANK, HARMONY_RAY, HARMONY_ORB, PARTICLE };

class GameObject {
public:
    Vec2 pos, vel;
    float radius = 10.0f;
    bool dead = false;
    ObjType type;
    Color color = Colors::WHITE;
    float angle = 0.0f;
    
    GameObject(Vec2 p, ObjType t) : pos(p), type(t) {}
    virtual ~GameObject() = default;
    virtual void update(float dt) {
        pos = pos + vel * dt;
    }
};

class Particle : public GameObject {
public:
    float life = 1.0f;
    float decay;
    Particle(Vec2 p, Color c, float spd, float decay_rate) 
        : GameObject(p, ObjType::PARTICLE), decay(decay_rate) {
        color = c;
        float a = rnd(0, std::numbers::pi_v<float>*2);
        vel = Vec2(std::cos(a), std::sin(a)) * spd;
        radius = rnd(2, 4);
    }
    void update(float dt) override {
        pos = pos + vel * dt;
        life -= decay * dt;
        if(life <= 0) dead = true;
        vel = vel * 0.95f;
    }
};

class HarmonyRay : public GameObject {
public:
    float damage = 10.0f;
    float life = 0.5f; // Fast, short-lived laser burst
    HarmonyRay(Vec2 p, Vec2 v, float dmg, Color c) : GameObject(p, ObjType::HARMONY_RAY) {
        vel = v;
        damage = dmg;
        color = c;
        radius = 2.0f;
    }
    void update(float dt) override {
        GameObject::update(dt);
        life -= dt;
        if(life <= 0) dead = true;
    }
};

class HarmonyOrb : public GameObject {
public:
    float value;
    HarmonyOrb(Vec2 p, float val) : GameObject(p, ObjType::HARMONY_ORB), value(val) {
        vel = Vec2(rnd(-30, 30), rnd(-30, 30));
        radius = 6.0f;
        color = Colors::NEON_GREEN;
    }
    void update(float dt) override {
        pos = pos + vel * dt;
        vel = vel * 0.92f;
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
    
    int ray_count = 1;
    float ray_speed = 1800.0f; // Very fast lasers
    float harmony_power = 15.0f;

    Player(Vec2 p) : GameObject(p, ObjType::PLAYER) {
        radius = 12.0f;
        color = Colors::NEON_BLUE;
    }

    void update(float dt) override {
        pos = pos + vel * dt;
        vel = vel * 0.92f;
        if (harmony_timer > 0) harmony_timer -= dt;
        if (dash_cooldown > 0) dash_cooldown -= dt;
    }
};

class Glitch : public GameObject {
public:
    float stability;
    float max_stability;

    Glitch(Vec2 p, ObjType t, float difficulty_mult) : GameObject(p, t) {
        float scale = 1.0f + difficulty_mult * 0.12f;
        if (t == ObjType::GLITCH_BASIC) {
            max_stability = stability = 30.0f * scale;
            radius = 14.0f;
            color = Colors::NEON_PINK;
        } else if (t == ObjType::GLITCH_TANK) {
            max_stability = stability = 100.0f * scale;
            radius = 24.0f;
            color = Colors::NEON_ORANGE; // Changed from RED to a warmer tone
        } else if (t == ObjType::GLITCH_DASH) {
            max_stability = stability = 20.0f * scale;
            radius = 10.0f;
            color = {200, 100, 255, 255}; // Purple dashers
        }
    }
};

inline std::expected<std::unique_ptr<Glitch>, std::string> create_glitch(Vec2 p, ObjType t, float diff) {
    if (t != ObjType::GLITCH_BASIC && t != ObjType::GLITCH_DASH && t != ObjType::GLITCH_TANK) {
        return std::unexpected("Invalid glitch type");
    }
    return std::make_unique<Glitch>(p, t, diff);
}