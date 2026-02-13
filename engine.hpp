#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <ranges>
#include <format>
#include <iostream> 
#include <chrono>
#include <future>
#include <generator>

#include "types.hpp"
#include "graphics.hpp"
#include "entities.hpp"
#include "audio.hpp"
#include "threading.hpp"
#include "quadtree.hpp"

#if __has_include(<print>)
#include <print>
#define HAS_PRINT 1
#else
#define HAS_PRINT 0
#endif

class Engine {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Graphics gfx;
    std::unique_ptr<AudioEngine> audio;
    WorkerPool workers{4};
    
    bool running = true;
    enum class State { MENU, PLAYING, EVOLUTION, GAME_OVER } state = State::MENU;

    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<GameObject>> entities;
    std::unique_ptr<QuadTree> qtree;
    
    float glitch_spawn_timer = 0.0f;
    float stability_timer = 0.0f;
    float screenshake = 0.0f;
    float hit_stop = 0.0f;

    struct GridPoint { Vec2 base; Vec2 offset; };
    std::vector<GridPoint> grid;
    const int GRID_SIZE = 30;
    const int WINDOW_W = 1200;
    const int WINDOW_H = 800;

    std::generator<Vec2> spawn_pattern_generator() {
        float angle = 0.0f;
        while (true) {
            int pattern = (int)(stability_timer / 15.0f) % 3;
            if (pattern == 0) { 
                angle += 0.4f;
                float r = 900.0f;
                co_yield Vec2(WINDOW_W/2 + std::cos(angle) * r, WINDOW_H/2 + std::sin(angle) * r);
            } else if (pattern == 1) { 
                static float tx = 0; tx += 0.5f;
                float x = WINDOW_W/2 + std::sin(tx) * WINDOW_W/2;
                float y = (rand() % 2) ? -60.0f : WINDOW_H + 60.0f;
                co_yield Vec2(x, y);
            } else { 
                float a = rnd(0, std::numbers::pi_v<float>*2);
                co_yield Vec2(WINDOW_W/2 + std::cos(a) * 800.0f, WINDOW_H/2 + std::sin(a) * 800.0f);
            }
        }
    }

    std::generator<Vec2> spawn_gen_instance = spawn_pattern_generator();
    using spawn_it_t = decltype(spawn_gen_instance.begin());
    spawn_it_t spawn_it = spawn_gen_instance.begin();

public:
    Engine() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
#if HAS_PRINT
            std::print("SDL Init Error: {}\n", SDL_GetError());
#else
            std::cout << "SDL Init Error: " << SDL_GetError() << std::endl;
#endif
        }
        
        audio = std::make_unique<AudioEngine>();
        if (!audio->start()) {
            std::cerr << "Failed to start Audio Engine" << std::endl;
        }

        TTF_Init();
        window = SDL_CreateWindow("NEON NEXUS: HARMONY", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        SDL_RenderSetLogicalSize(renderer, WINDOW_W, WINDOW_H);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD); 
        gfx.r = renderer;

        gfx.font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 64);
        if (!gfx.font) gfx.font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 64);

        for(int y = 0; y <= WINDOW_H; y += GRID_SIZE) {
            for(int x = 0; x <= WINDOW_W; x += GRID_SIZE) {
                grid.push_back({Vec2((float)x, (float)y), Vec2(0,0)});
            }
        }
        
        qtree = std::make_unique<QuadTree>(Rect{0, 0, (float)WINDOW_W, (float)WINDOW_H});
        reset_game();
    }

    ~Engine() {
        audio.reset();
        if (gfx.font) TTF_CloseFont(gfx.font);
        TTF_Quit();
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void reset_game() {
        entities.clear();
        player = std::make_unique<Player>(Vec2(WINDOW_W/2, WINDOW_H/2));
        glitch_spawn_timer = 0;
        stability_timer = 0;
        screenshake = 0;
        if (audio) audio->set_binaural(150.0f, 10.0f, 0.2f);
    }

    void spawn_particle(Vec2 p, Color c, int count, float speed = 100.0f) {
        for(int i=0; i<count; ++i) {
            entities.push_back(std::make_unique<Particle>(p, c, rnd(speed*0.5f, speed*1.5f), rnd(1.0f, 3.0f)));
        }
    }

    void apply_force_to_grid(Vec2 pos, float force, float radius) {
        for(auto& p : grid) {
            float d = p.base.dist(pos);
            if(d < radius) {
                Vec2 dir = (p.base - pos).norm();
                float f = (1.0f - d / radius) * force;
                p.offset = p.offset + dir * f;
            }
        }
    }

    void update_grid_parallel() {
        const size_t num_points = grid.size();
        const size_t chunk_size = num_points / 4;
        std::vector<std::future<void>> results;
        for (int i = 0; i < 4; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == 3) ? num_points : (i + 1) * chunk_size;
            results.push_back(workers.enqueue([this, start, end] {
                for (size_t j = start; j < end; ++j) {
                    grid[j].offset = grid[j].offset * 0.92f;
                }
            }));
        }
        for (auto& f : results) f.wait();
    }

    void update(float dt) {
        if (state != State::PLAYING) return;
        stability_timer += dt;
        
        if (audio) {
            float beat_freq = 10.0f + (stability_timer / 12.0f);
            float carrier_freq = 150.0f + std::sin(stability_timer * 0.05f) * 30.0f;
            audio->set_binaural(carrier_freq, std::min(beat_freq, 60.0f), 0.2f + std::min(stability_timer * 0.0005f, 0.1f));
        }

        if (screenshake > 0) screenshake -= dt * 30.0f;
        else screenshake = 0;
        gfx.cam_offset = Vec2(rnd(-screenshake, screenshake), rnd(-screenshake, screenshake));

        const Uint8* keys = SDL_GetKeyboardState(NULL);
        Vec2 move_input;
        if (keys[SDL_SCANCODE_W]) move_input.y -= 1;
        if (keys[SDL_SCANCODE_S]) move_input.y += 1;
        if (keys[SDL_SCANCODE_A]) move_input.x -= 1;
        if (keys[SDL_SCANCODE_D]) move_input.x += 1;
        if (move_input.mag() > 0) player->vel = player->vel + move_input.norm() * player->speed * 8.0f * dt;

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        player->angle = std::atan2(static_cast<float>(my) - player->pos.y, static_cast<float>(mx) - player->pos.x);

        if (keys[SDL_SCANCODE_SPACE] && player->dash_cooldown <= 0) {
            player->vel = move_input.norm() * 1400.0f;
            player->dash_cooldown = 1.0f;
            screenshake = 6.0f;
            spawn_particle(player->pos, Colors::NEON_BLUE, 15);
            if (audio) audio->play_sfx(180.0f, 0.3f, 0.15f); // Soft dash whoosh
        }

        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT) && player->harmony_timer <= 0) {
            player->harmony_timer = player->harmony_rate;
            Vec2 fwd(std::cos(player->angle), std::sin(player->angle));
            player->vel = player->vel - fwd * 60.0f;
            screenshake = 2.0f;
            if (audio) audio->play_sfx(1500.0f + rnd(-100, 100), 0.06f, 0.04f); // High chime for lasers

            for (int i = 0; i < player->ray_count; ++i) {
                float angle_offset = (i - (player->ray_count-1)/2.0f) * 0.12f;
                Vec2 dir(std::cos(player->angle + angle_offset), std::sin(player->angle + angle_offset));
                entities.push_back(std::make_unique<HarmonyRay>(player->pos + dir * 20, dir * player->ray_speed, player->harmony_power, Colors::NEON_BLUE));
            }
        }

        player->update(dt);
        update_grid_parallel();

        glitch_spawn_timer -= dt;
        if (glitch_spawn_timer <= 0) {
            float difficulty = stability_timer / 30.0f;
            glitch_spawn_timer = 1.3f / (1.0f + difficulty * 0.4f);
            Vec2 spawn_pos = *spawn_it;
            ++spawn_it;
            ObjType t = ObjType::GLITCH_BASIC;
            if (difficulty > 1.0f && rand()%4==0) t = ObjType::GLITCH_TANK;
            if (difficulty > 2.0f && rand()%4==0) t = ObjType::GLITCH_DASH;
            auto glitch_res = create_glitch(spawn_pos, t, difficulty);
            if (glitch_res) entities.push_back(std::move(*glitch_res));
        }

        qtree->clear();
        for (auto& e : entities) {
            if (e->type != ObjType::PARTICLE) qtree->insert(e.get());
        }

        for (auto& e : entities) {
            e->update(dt);
            if (e->dead) continue;

            if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_DASH) {
                Vec2 dir = (player->pos - e->pos).norm();
                float move_speed = (e->type == ObjType::GLITCH_TANK) ? 50.0f : ((e->type == ObjType::GLITCH_DASH) ? 170.0f : 90.0f);
                e->vel = e->vel + dir * move_speed * dt * 2.0f;
                e->vel = e->vel * 0.95f;

                if (e->pos.dist(player->pos) < e->radius + player->radius) {
                    player->integrity -= 8;
                    screenshake = 10.0f;
                    hit_stop = 0.08f;
                    spawn_particle(player->pos, {255, 200, 100, 255}, 20); // Golden sparkle on hit
                    apply_force_to_grid(player->pos, 40.0f, 180.0f);
                    if (audio) audio->play_sfx(220.0f, 0.4f, 0.3f); // Soft resonant pulse
                    e->dead = true;
                    if (player->integrity <= 0) state = State::GAME_OVER;
                }
            }
            
            if (e->type == ObjType::HARMONY_RAY) {
                float r = e->radius + 25.0f;
                std::vector<GameObject*> targets;
                qtree->query(Rect{e->pos.x - r, e->pos.y - r, r*2, r*2}, targets);
                for (auto target : targets) {
                    if (target->type == ObjType::GLITCH_BASIC || target->type == ObjType::GLITCH_TANK || target->type == ObjType::GLITCH_DASH) {
                        if (e->pos.dist(target->pos) < target->radius + e->radius) {
                            Glitch* g = static_cast<Glitch*>(target);
                            g->stability -= static_cast<HarmonyRay*>(e.get())->damage;
                            e->dead = true;
                            spawn_particle(e->pos, Colors::NEON_BLUE, 3);
                            if (g->stability <= 0) {
                                g->dead = true;
                                spawn_particle(g->pos, g->color, 18);
                                apply_force_to_grid(g->pos, 25.0f, 130.0f);
                                if (audio) audio->play_sfx(440.0f + rnd(0, 440), 0.15f, 0.2f); // Musical chime on harmonization
                                entities.push_back(std::make_unique<HarmonyOrb>(g->pos, 10.0f));
                            }
                            break;
                        }
                    }
                }
            }

            if (e->type == ObjType::HARMONY_ORB) {
                float dist = e->pos.dist(player->pos);
                if (dist < 160.0f) e->vel = e->vel + (player->pos - e->pos).norm() * 1100.0f * dt;
                if (dist < player->radius + 12.0f) {
                    e->dead = true;
                    player->harmony_xp += static_cast<HarmonyOrb*>(e.get())->value;
                    if (player->harmony_xp >= player->harmony_next) {
                        player->harmony_xp = 0; player->harmony_next *= 1.25f; player->level++;
                        state = State::EVOLUTION;
                        if (audio) audio->play_sfx(880.0f, 0.5f, 0.5f);
                    }
                }
            }
        }
        std::erase_if(entities, [](const auto& e) { return e->dead; });
    }

    void render() {
        SDL_SetRenderDrawColor(renderer, 10, 10, 25, 255); // Slightly bluer background
        SDL_RenderClear(renderer);
        gfx.set_color({50, 100, 255, 40});
        int cols = (WINDOW_W / GRID_SIZE) + 1;
        for(size_t i = 0; i < grid.size() - 1; ++i) {
            if ((i + 1) % cols == 0) continue;
            gfx.draw_line(grid[i].base + grid[i].offset, grid[i+1].base + grid[i+1].offset);
        }
        for(size_t i = 0; i < grid.size() - cols; ++i) {
            gfx.draw_line(grid[i].base + grid[i].offset, grid[i+cols].base + grid[i+cols].offset);
        }
        if (state == State::PLAYING || state == State::EVOLUTION || state == State::GAME_OVER) {
            for (auto& e : entities) {
                if (e->type == ObjType::PARTICLE) {
                    auto* p = static_cast<Particle*>(e.get());
                    gfx.set_color(p->color, p->life);
                    gfx.draw_circle(p->pos, p->radius * p->life);
                } else if (e->type == ObjType::HARMONY_RAY) {
                    // Draw laser beam
                    gfx.set_color(e->color, 0.8f);
                    Vec2 fwd = e->vel.norm() * 40.0f;
                    gfx.draw_line(e->pos, e->pos - fwd);
                    gfx.draw_glowing_circle(e->pos, e->radius, e->color);
                } else {
                    gfx.draw_glowing_circle(e->pos, e->radius, e->color);
                }
            }
            Vec2 dir(std::cos(player->angle), std::sin(player->angle));
            Vec2 nose = player->pos + dir * 20.0f;
            Vec2 l_wing = player->pos + dir * -10.0f + Vec2(-dir.y, dir.x) * 12.0f;
            Vec2 r_wing = player->pos + dir * -10.0f + Vec2(dir.y, -dir.x) * 12.0f;
            gfx.set_color(player->color);
            gfx.draw_line(nose, l_wing); gfx.draw_line(l_wing, r_wing); gfx.draw_line(r_wing, nose);
            gfx.draw_text(std::format("INTEGRITY: {:.0f}", player->integrity), {20, 20}, 20, {255, 150, 50, 255});
            gfx.draw_text(std::format("HARMONY: {:.0f}/{:.0f}", player->harmony_xp, player->harmony_next), {20, 50}, 20, Colors::NEON_GREEN);
            gfx.draw_text(std::format("EVOLUTION: {}", player->level), {20, 80}, 20, Colors::WHITE);
        }
        if (state == State::MENU) {
            gfx.draw_text_centered("NEON NEXUS: HARMONY", {WINDOW_W/2, 200}, 80, Colors::NEON_BLUE);
            gfx.draw_text_centered("PRESS SPACE TO RESTORE ORDER", {WINDOW_W/2, 400}, 30, Colors::WHITE);
        } else if (state == State::EVOLUTION) {
            gfx.draw_text_centered("EVOLUTION REACHED", {WINDOW_W/2, 200}, 60, Colors::NEON_GREEN);
            gfx.draw_text_centered("1: RAY DENSITY  2: HARMONY POWER  3: RESONANCE RATE", {WINDOW_W/2, 400}, 30, Colors::WHITE);
        } else if (state == State::GAME_OVER) {
            gfx.draw_text_centered("NEXUS DEFLATED", {WINDOW_W/2, 300}, 80, {255, 100, 50, 255});
            gfx.draw_text_centered("SPACE TO TRY AGAIN", {WINDOW_W/2, 450}, 30, Colors::WHITE);
        }
        SDL_RenderPresent(renderer);
    }

    void run() {
        auto last_time = std::chrono::high_resolution_clock::now();
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_time).count();
            last_time = now;
            if (dt > 0.05f) dt = 0.05f;
            if (hit_stop > 0) { hit_stop -= dt; dt = 0; }
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (state == State::MENU && ev.key.keysym.sym == SDLK_SPACE) state = State::PLAYING;
                    if (state == State::GAME_OVER && ev.key.keysym.sym == SDLK_SPACE) { reset_game(); state = State::PLAYING; }
                    if (state == State::EVOLUTION) {
                        if (ev.key.keysym.sym == SDLK_1) { player->ray_count++; state = State::PLAYING; }
                        if (ev.key.keysym.sym == SDLK_2) { player->harmony_power *= 1.35f; state = State::PLAYING; }
                        if (ev.key.keysym.sym == SDLK_3) { player->harmony_rate *= 0.85f; state = State::PLAYING; }
                    }
                }
            }
            update(dt);
            render();
        }
    }
};