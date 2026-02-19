#pragma once
#include "types.hpp"
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <print>

class Graphics {
public:
    SDL_Renderer* r;
    Vec2 cam_offset;
    TTF_Font* font = nullptr;
    
    // Optimization: Precomputed circle vertices
    static constexpr int CIRCLE_SEGMENTS = 64;
    std::vector<Vec2> unit_circle;

    Graphics() {
        unit_circle.reserve(CIRCLE_SEGMENTS);
        for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
            float theta = 2.0f * std::numbers::pi_v<float> * i / CIRCLE_SEGMENTS;
            unit_circle.push_back(Vec2(std::cos(theta), std::sin(theta)));
        }
    }

    void set_color(Color c, float alpha_mod = 1.0f) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<Uint8>(c.a * alpha_mod));
    }

    void draw_line(Vec2 a, Vec2 b) {
        SDL_RenderDrawLineF(r, a.x - cam_offset.x, a.y - cam_offset.y, b.x - cam_offset.x, b.y - cam_offset.y);
    }

    void draw_circle(Vec2 pos, float radius) {
        if (radius < 0.5f) return;
        // Optimization: Use precomputed unit circle
        static std::vector<SDL_FPoint> points;
        points.clear();
        
        // Adaptive Level of Detail (LOD) based on radius
        int step = 1;
        if (radius < 5.0f) step = 8;       // 8 segments
        else if (radius < 15.0f) step = 4; // 16 segments
        else if (radius < 30.0f) step = 2; // 32 segments
        
        for (int i = 0; i < CIRCLE_SEGMENTS; i += step) {
            points.push_back({
                pos.x - cam_offset.x + unit_circle[i].x * radius,
                pos.y - cam_offset.y + unit_circle[i].y * radius
            });
        }
        points.push_back(points[0]); 
        SDL_RenderDrawLinesF(r, points.data(), points.size());
    }

    void draw_glowing_circle(Vec2 pos, float radius, Color c) {
        set_color(c, 0.15f);
        draw_circle(pos, radius + 3.0f);
        set_color(c, 0.4f);
        draw_circle(pos, radius);
        set_color(Colors::WHITE, 0.8f);
        draw_circle(pos, radius * 0.6f);
    }

    void draw_bloom(Vec2 pos, float radius, Color c) {
        // Optimization: Reduce bloom layers for performance
        for (int i = 0; i < 3; ++i) {
            float r = radius * (1.0f + i * 0.5f);
            float a = 0.2f / (i + 1);
            set_color(c, a);
            draw_circle(pos, r);
        }
    }

    void draw_wireframe_3d(Vec2 pos, const std::vector<Vec3>& vertices, const std::vector<std::pair<int, int>>& edges, float angle_x, float angle_y, float angle_z, float scale, Color c) {
        set_color(c);
        // Optimization: Stack allocation for projected points to avoid heap churn
        static std::vector<Vec2> projected;
        projected.clear();
        projected.reserve(vertices.size());
        
        float fov = 400.0f;
        float view_dist = 300.0f;

        // Precompute rotation matrices could be faster, but this is okay for low vertex counts
        float sx = std::sin(angle_x), cx = std::cos(angle_x);
        float sy = std::sin(angle_y), cy = std::cos(angle_y);
        float sz = std::sin(angle_z), cz = std::cos(angle_z);

        for (const auto& v : vertices) {
            // Manual rotation expansion to avoid function call overhead
            float y1 = v.y * cx - v.z * sx;
            float z1 = v.y * sx + v.z * cx;
            float x2 = v.x * cy + z1 * sy;
            float z2 = -v.x * sy + z1 * cy;
            float x3 = x2 * cz - y1 * sz;
            float y3 = x2 * sz + y1 * cz;

            float z = z2 * scale + view_dist;
            if (z < 1.0f) z = 1.0f; // Prevent divide by zero
            float factor = fov / z;
            projected.push_back(Vec2(pos.x + x3 * scale * factor, pos.y + y3 * scale * factor));
        }

        for (const auto& edge : edges) {
            draw_line(projected[edge.first], projected[edge.second]);
        }
    }

    void draw_filled_rect(float x, float y, float w, float h, Color c) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        SDL_Rect rect = { static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h) };
        SDL_RenderFillRect(r, &rect);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    }

    void draw_rect(float x, float y, float w, float h, Color c) {
        set_color(c);
        SDL_Rect rect = { static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h) };
        SDL_RenderDrawRect(r, &rect);
    }

    void draw_cross(Vec2 pos, float size, Color c, float thickness = 2.0f, bool glow = false) {
        if (glow) {
            for(int i=0; i<3; ++i) {
                float s = size * (1.0f + i * 0.2f);
                float a = 0.3f / (i + 1);
                draw_filled_rect(pos.x - thickness*2, pos.y - s, thickness * 4, s * 2.5f, {c.r, c.g, c.b, (Uint8)(c.a * a)});
                draw_filled_rect(pos.x - s, pos.y - s * 0.3f, s * 2, thickness * 4, {c.r, c.g, c.b, (Uint8)(c.a * a)});
            }
        }
        set_color(c);
        draw_filled_rect(pos.x - thickness, pos.y - size, thickness * 2, size * 2.5f, c);
        draw_filled_rect(pos.x - size, pos.y - size * 0.3f, size * 2, thickness * 2, c);
    }

    void draw_vignette(int w, int h, Color c, float intensity) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        float thickness = 45.0f; 
        // Optimization: Reduced to 4 steps from 6
        for(int i=0; i<4; ++i) {
            float pct = i / 4.0f;
            Uint8 a = static_cast<Uint8>(intensity * (1.0f - pct) * 20.0f); 
            draw_filled_rect(0, 0, w, thickness * (1.0f-pct), {c.r, c.g, c.b, a}); 
            draw_filled_rect(0, h - thickness * (1.0f-pct), w, thickness * (1.0f-pct), {c.r, c.g, c.b, a}); 
            draw_filled_rect(0, 0, thickness * (1.0f-pct), h, {c.r, c.g, c.b, a}); 
            draw_filled_rect(w - thickness * (1.0f-pct), 0, thickness * (1.0f-pct), h, {c.r, c.g, c.b, a}); 
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    }

    void draw_indicator(Vec2 pos, float angle, Color c) {
        float size = 12.0f;
        Vec2 fwd(std::cos(angle), std::sin(angle));
        Vec2 side(-fwd.y, fwd.x);
        Vec2 p1 = pos + fwd * size;
        Vec2 p2 = pos - fwd * size * 0.5f + side * size * 0.5f;
        Vec2 p3 = pos - fwd * size * 0.5f - side * size * 0.5f;
        
        set_color(c, 0.6f);
        draw_line(p1, p2);
        draw_line(p2, p3);
        draw_line(p3, p1);
    }

    void draw_scanlines(int w, int h, float time) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 40);
        int spacing = 4;
        int offset = (int)(time * 20.0f) % spacing;
        for (int y = offset; y < h; y += spacing) {
            SDL_Rect line = { 0, y, w, 1 };
            SDL_RenderFillRect(r, &line);
        }
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
    }

    void draw_chromatic_aberration(float intensity) {
        if (intensity <= 0) return;
        // This is a simple visual trick: draw a slightly shifted cyan/magenta copy of the screen
        // In a real post-processing shader this would be better, but we can simulate it with bloom
        // by drawing extra pulses or lines. For this prototype, we'll implement it as a camera shake
        // of color layers if we had a backbuffer. Since we don't, we'll provide a hook for Engine.
    }
    
    void draw_text(const std::string& text, Vec2 pos, float size, Color c) {
        if (!font) return;
        SDL_Color sdl_c = {c.r, c.g, c.b, c.a};
        SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), sdl_c);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_Rect dst = { (int)pos.x, (int)pos.y, surf->w, surf->h };
        float scale = size / surf->h;
        dst.w = (int)(surf->w * scale);
        dst.h = (int)size;
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    void draw_text_centered(const std::string& text, Vec2 pos, float size, Color c) {
        if (!font) return;
        SDL_Color sdl_c = {c.r, c.g, c.b, c.a};
        SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), sdl_c);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        float scale = size / surf->h;
        int w = (int)(surf->w * scale);
        SDL_Rect dst = { (int)pos.x - w/2, (int)pos.y - (int)size/2, w, (int)size };
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }

    void draw_text_centered_wrapped(const std::string& text, Vec2 pos, float size, Color c, Uint32 wrap_w) {
        if (!font) return;
        SDL_Color sdl_c = {c.r, c.g, c.b, c.a};
        // We need to scale wrap_w because TTF_RenderText_Blended_Wrapped uses font pixels
        Uint32 scaled_wrap = static_cast<Uint32>(wrap_w * (64.0f / size));
        SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(font, text.c_str(), sdl_c, scaled_wrap);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        float scale = size / 64.0f; // Standardized scale
        int w = static_cast<int>(surf->w * scale);
        int h = static_cast<int>(surf->h * scale);
        SDL_Rect dst = { (int)pos.x - w/2, (int)pos.y - h/2, w, h };
        SDL_RenderCopy(r, tex, NULL, &dst);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(tex);
    }
};