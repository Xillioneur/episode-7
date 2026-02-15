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

    void set_color(Color c, float alpha_mod = 1.0f) {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<Uint8>(c.a * alpha_mod));
    }

    void draw_line(Vec2 a, Vec2 b) {
        SDL_RenderDrawLineF(r, a.x - cam_offset.x, a.y - cam_offset.y, b.x - cam_offset.x, b.y - cam_offset.y);
    }

    void draw_circle(Vec2 pos, float radius) {
        if (radius < 0.5f) return;
        std::vector<SDL_FPoint> points;
        int segments = 16 + static_cast<int>(radius * 0.5f);
        for (int i = 0; i < segments; ++i) {
            float theta = 2.0f * std::numbers::pi_v<float> * i / segments;
            points.push_back({
                pos.x - cam_offset.x + radius * std::cos(theta),
                pos.y - cam_offset.y + radius * std::sin(theta)
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
        for (int i = 0; i < 4; ++i) {
            float r = radius * (1.0f + i * 0.5f);
            float a = 0.2f / (i + 1);
            set_color(c, a);
            draw_circle(pos, r);
        }
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
};