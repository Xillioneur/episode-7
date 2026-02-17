#pragma once
#include <SDL.h>
#include <cmath>
#include <random>
#include <numbers>
#include <algorithm>

struct Vec2 {
    float x, y;
    constexpr Vec2(float x = 0, float y = 0) : x(x), y(y) {}
    constexpr Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    constexpr Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    constexpr Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    constexpr Vec2 operator/(float s) const { return s == 0 ? Vec2() : Vec2(x / s, y / s); }
    float mag() const { return std::hypot(x, y); }
    Vec2 norm() const { float m = mag(); return m > 0 ? *this / m : Vec2(); }
    float dist(const Vec2& o) const { return (*this - o).mag(); }
};

struct Vec3 {
    float x, y, z;
    constexpr Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    constexpr Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    constexpr Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    constexpr Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    
    Vec3 rotate_x(float a) const {
        float s = std::sin(a), c = std::cos(a);
        return Vec3(x, y * c - z * s, y * s + z * c);
    }
    Vec3 rotate_y(float a) const {
        float s = std::sin(a), c = std::cos(a);
        return Vec3(x * c + z * s, y, -x * s + z * c);
    }
    Vec3 rotate_z(float a) const {
        float s = std::sin(a), c = std::cos(a);
        return Vec3(x * c - y * s, x * s + y * c, z);
    }
};

struct Color { 
    Uint8 r, g, b, a; 
    constexpr Color() : r(0), g(0), b(0), a(255) {} 
    constexpr Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) : r(r), g(g), b(b), a(a) {}
};

namespace Colors {
    inline constexpr Color WHITE{255, 255, 255, 255};
    inline constexpr Color GOLD{255, 215, 0, 255};
    inline constexpr Color DIVINE_WHITE{240, 245, 255, 255};
    inline constexpr Color HOLY_GOLD{255, 230, 100, 255};
    inline constexpr Color RADIANT_TEAL{0, 255, 200, 255};
    inline constexpr Color VOID_PURPLE{80, 0, 120, 255};
    inline constexpr Color SHADOW_DARK{40, 0, 60, 255};
    inline constexpr Color RED{255, 50, 50, 255};
    inline constexpr Color BG_DARK{5, 5, 20, 255};
}

inline float rnd(float min, float max) {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

inline Color lerp_color(Color a, Color b, float t) {
    return Color(
        (Uint8)std::lerp((float)a.r, (float)b.r, t),
        (Uint8)std::lerp((float)a.g, (float)b.g, t),
        (Uint8)std::lerp((float)a.b, (float)b.b, t),
        (Uint8)std::lerp((float)a.a, (float)b.a, t)
    );
}