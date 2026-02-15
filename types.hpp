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

struct Color { 
    Uint8 r, g, b, a; 
    constexpr Color() : r(0), g(0), b(0), a(255) {} // Default constructor
    constexpr Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) : r(r), g(g), b(b), a(a) {}
};

namespace Colors {
    inline constexpr Color WHITE{255, 255, 255, 255};
    inline constexpr Color NEON_BLUE{0, 255, 255, 255};
    inline constexpr Color NEON_PINK{255, 0, 128, 255};
    inline constexpr Color NEON_ORANGE{255, 100, 0, 255};
    inline constexpr Color NEON_GREEN{50, 255, 50, 255};
    inline constexpr Color RED{255, 50, 50, 255};
    inline constexpr Color GOLD{255, 215, 0, 255};
    inline constexpr Color BG_DARK{10, 10, 25, 255};
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