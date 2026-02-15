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
#include <optional>

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

struct BloomPulse {
    Vec2 pos;
    float radius = 0.0f;
    float max_radius;
    float life = 1.0f;
    Color color;
};

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
    std::vector<std::unique_ptr<GameObject>> new_entities; 
    std::vector<BloomPulse> pulses;
    std::unique_ptr<QuadTree> qtree;
    
    static constexpr int MAX_PARTICLES = 2000;
    std::array<ParticleData, MAX_PARTICLES> particles;
    int particle_ptr = 0;
    
    float glitch_spawn_timer = 0.0f;
    float stability_timer = 0.0f;
    float boss_spawn_timer = 60.0f; 
    float screenshake = 0.0f;
    float hit_stop = 0.0f;
    float low_integrity_timer = 0.0f;
    float scatter_timer = 0.0f;

    struct GridPoint { Vec2 base; Vec2 offset; Vec2 vel; };
    std::vector<GridPoint> grid;
    const int GRID_SIZE = 35;
    const int WINDOW_W = 1200;
    const int WINDOW_H = 800;

    Vec2 generate_spawn_pos() {
        static float angle = 0.0f;
        int pattern = (int)(stability_timer / 15.0f) % 3;
        if (pattern == 0) { 
            angle += 0.4f;
            float r = 900.0f;
            return Vec2(WINDOW_W/2 + std::cos(angle) * r, WINDOW_H/2 + std::sin(angle) * r);
        } else if (pattern == 1) { 
            static float tx = 0; tx += 0.5f;
            float x = WINDOW_W/2 + std::sin(tx) * WINDOW_W/2;
            float y = (rand() % 2) ? -60.0f : WINDOW_H + 60.0f;
            return Vec2(x, y);
        } else { 
            float a = rnd(0, std::numbers::pi_v<float>*2);
            return Vec2(WINDOW_W/2 + std::cos(a) * 800.0f, WINDOW_H/2 + std::sin(a) * 800.0f);
        }
    }

public:
    Engine() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        }
        audio = std::make_unique<AudioEngine>();
        if (!audio->start()) std::cerr << "Warning: Audio system offline." << std::endl;
        TTF_Init();
        window = SDL_CreateWindow("NEON NEXUS: HARMONY", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        SDL_RenderSetLogicalSize(renderer, WINDOW_W, WINDOW_H);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD); 
        gfx.r = renderer;
        gfx.font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 64);
        if (!gfx.font) gfx.font = TTF_OpenFont("/Library/Fonts/Arial.ttf", 64);
        for(int y = 0; y <= WINDOW_H; y += GRID_SIZE) {
            for(int x = 0; x <= WINDOW_W; x += GRID_SIZE) {
                grid.push_back({Vec2((float)x, (float)y), Vec2(0,0), Vec2(0,0)});
            }
        }
        qtree = std::make_unique<QuadTree>(Rect{0, 0, (float)WINDOW_W, (float)WINDOW_H});
        reset_game();
    }

    ~Engine() {
        audio.reset();
        if (gfx.font) TTF_CloseFont(gfx.font);
        TTF_Quit();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void reset_game() {
        entities.clear();
        new_entities.clear();
        pulses.clear();
        for(auto& p : particles) p.active = false;
        player = std::make_unique<Player>(Vec2(WINDOW_W/2, WINDOW_H/2));
        glitch_spawn_timer = 0;
        stability_timer = 0;
        boss_spawn_timer = 60.0f;
        screenshake = 0;
        low_integrity_timer = 0;
        scatter_timer = 0;
        if (audio) {
            audio->set_binaural(432.0f, 40.0f, 0.15f);
            audio->set_player_state(0.5f, 0.0f, false, 0.0f);
        }
    }

    void spawn_particle_chaos(Vec2 p, Color c, int count, float speed = 100.0f) {
        for(int i=0; i<count; ++i) {
            auto& d = particles[particle_ptr];
            d.active = true; d.pos = p;
            float a = rnd(0, std::numbers::pi_v<float>*2);
            d.vel = Vec2(std::cos(a), std::sin(a)) * rnd(speed*0.5f, speed*1.5f);
            d.color = c; d.life = 1.0f; d.decay = rnd(1.0f, 3.0f); d.radius = rnd(1.5f, 3.5f);
            particle_ptr = (particle_ptr + 1) % MAX_PARTICLES;
        }
    }

    void add_bloom(Vec2 p, float r, Color c) {
        pulses.push_back({p, 0.0f, r, 1.0f, c});
    }

    void apply_force_to_grid(Vec2 pos, float force, float radius) {
        for(auto& p : grid) {
            float d = p.base.dist(pos + p.offset);
            if(d < radius) {
                Vec2 dir = ( (p.base + p.offset) - pos).norm();
                float f = (1.0f - d / radius) * force;
                p.vel = p.vel + dir * f;
            }
        }
    }

    void update_grid_parallel(float dt) {
        const float spring_k = 15.0f;
        const float damp = 0.92f;
        const size_t num_points = grid.size();
        const size_t chunk_size = num_points / 4;
        float music_kick = 0.0f;
        if (audio) music_kick = audio->get_music_state().intensity;
        std::vector<std::future<void>> results;
        for (int i = 0; i < 4; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i == 3) ? num_points : (i + 1) * chunk_size;
            results.push_back(workers.enqueue([this, start, end, dt, spring_k, damp, music_kick] {
                for (size_t j = start; j < end; ++j) {
                    auto& p = grid[j];
                    Vec2 spring_force = p.offset * -spring_k;
                    if (music_kick > 0.5f) {
                        float d = p.base.dist(player->pos);
                        spring_force = spring_force + (p.base - player->pos).norm() * (music_kick * 20.0f / (1.0f + d * 0.01f));
                    }
                    p.vel = p.vel + spring_force * dt;
                    p.offset = p.offset + p.vel * dt;
                    p.vel = p.vel * damp;
                }
            }));
        }
        for (auto& f : results) f.wait();
    }

    void update(float dt) {
        if (audio) {
            float x_norm = player->pos.x / (float)WINDOW_W;
            float speed_norm = player->vel.mag() / player->speed;
            float progress = player->harmony_xp / player->harmony_next;
            audio->set_player_state(x_norm, speed_norm, state == State::PLAYING, progress);
        }

        if (state != State::PLAYING) return;
        stability_timer += dt;
        
        scatter_timer += dt;
        if (scatter_timer >= rnd(2.5f, 5.0f)) {
            scatter_timer = 0;
            if (audio) audio->play_randomized_sfx(2500.0f + rnd(0, 1500), 0.015f, 0.4f, false, rnd(0,1));
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
        if (move_input.mag() > 0) {
            Vec2 move_dir = move_input.norm();
            player->vel = player->vel + move_dir * player->speed * 8.0f * dt;
            apply_force_to_grid(player->pos, 12.0f, 120.0f);
        }

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        player->angle = std::atan2(static_cast<float>(my) - player->pos.y, static_cast<float>(mx) - player->pos.x);

        if (keys[SDL_SCANCODE_SPACE] && player->dash_cooldown <= 0) {
            player->vel = move_input.norm() * 1500.0f;
            player->dash_cooldown = 1.0f;
            screenshake = 15.0f;
            spawn_particle_chaos(player->pos, Colors::NEON_BLUE, 30);
            apply_force_to_grid(player->pos, 180.0f, 300.0f);
            add_bloom(player->pos, 250.0f, Colors::NEON_BLUE);
            if (audio) audio->play_warp_jump(); // TRIGGER WARP JUMP
        }

        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT) && player->harmony_timer <= 0) {
            player->harmony_timer = player->harmony_rate;
            Vec2 fwd(std::cos(player->angle), std::sin(player->angle));
            player->vel = player->vel - fwd * 70.0f;
            screenshake = 2.5f;
            if (audio) audio->play_divine_ray();
            for (int i = 0; i < player->ray_count; ++i) {
                float angle_offset = (i - (player->ray_count-1)/2.0f) * player->ray_spread;
                Vec2 dir(std::cos(player->angle + angle_offset), std::sin(player->angle + angle_offset));
                new_entities.push_back(std::make_unique<HarmonyRay>(player->pos + dir * 20, dir * player->ray_speed, player->harmony_power, Colors::NEON_BLUE, player->ray_pierce));
            }
        }

        player->update(dt);
        update_grid_parallel(dt);

        for(auto& p : particles) {
            if(!p.active) continue;
            p.pos = p.pos + p.vel * dt; p.life -= p.decay * dt; p.vel = p.vel * 0.96f;
            if(p.life <= 0) p.active = false;
        }

        for(auto& p : pulses) { p.radius += 600.0f * dt; p.life -= dt * 1.8f; }
        std::erase_if(pulses, [](const auto& p) { return p.life <= 0; });

        glitch_spawn_timer -= dt;
        if (glitch_spawn_timer <= 0) {
            float difficulty = stability_timer / 30.0f;
            glitch_spawn_timer = 1.2f / (1.0f + difficulty * 0.45f);
            Vec2 spawn_pos = generate_spawn_pos();
            ObjType t = ObjType::GLITCH_BASIC;
            if (difficulty > 1.0f && rand()%4==0) t = ObjType::GLITCH_TANK;
            if (difficulty > 2.0f && rand()%4==0) t = ObjType::GLITCH_DASH;
            auto glitch_res = create_glitch(spawn_pos, t, difficulty);
            if (glitch_res) {
                new_entities.push_back(std::move(*glitch_res));
                apply_force_to_grid(spawn_pos, 60.0f, 180.0f);
                if (audio && (t == ObjType::GLITCH_TANK)) audio->play_glitch_spawn();
            }
        }

        boss_spawn_timer -= dt;
        if (boss_spawn_timer <= 0) {
            boss_spawn_timer = 60.0f;
            Vec2 spawn_pos = generate_spawn_pos();
            auto boss_res = create_glitch(spawn_pos, ObjType::GLITCH_BOSS, stability_timer / 30.0f);
            if (boss_res) {
                new_entities.push_back(std::move(*boss_res));
                add_bloom(spawn_pos, 600.0f, Colors::NEON_ORANGE);
                apply_force_to_grid(spawn_pos, 400.0f, 700.0f);
                if (audio) audio->play_glitch_spawn(); 
                screenshake = 30.0f;
            }
        }

        qtree->clear();
        for (auto& e : entities) { if (e->type != ObjType::PARTICLE) qtree->insert(e.get()); }

        for (auto& e : entities) {
            e->update(dt);
            if (e->dead) continue;

            if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_BOSS) {
                Vec2 dir = (player->pos - e->pos).norm();
                float move_speed = (e->type == ObjType::GLITCH_BOSS) ? 45.0f : ((e->type == ObjType::GLITCH_TANK) ? 55.0f : ((e->type == ObjType::GLITCH_DASH) ? 180.0f : 100.0f));
                e->vel = e->vel + dir * move_speed * dt * 2.2f;
                e->vel = e->vel * 0.95f;
                if (e->pos.dist(player->pos) < e->radius + player->radius) {
                    player->integrity -= (e->type == ObjType::GLITCH_BOSS) ? 35 : 10;
                    screenshake = (e->type == ObjType::GLITCH_BOSS) ? 35.0f : 12.0f;
                    hit_stop = 0.1f;
                    spawn_particle_chaos(player->pos, Colors::GOLD, 40);
                    apply_force_to_grid(player->pos, 100.0f, 250.0f);
                    if (audio) audio->play_randomized_sfx(216.0f, 0.4f, 0.4f, true, player->pos.x / (float)WINDOW_W);
                    e->dead = (e->type != ObjType::GLITCH_BOSS); 
                    if (player->integrity <= 0) state = State::GAME_OVER;
                }
            }
            
            if (e->type == ObjType::HARMONY_RAY) {
                HarmonyRay* ray = static_cast<HarmonyRay*>(e.get());
                float r = ray->radius + 30.0f;
                std::vector<GameObject*> targets;
                qtree->query(Rect{ray->pos.x - r, ray->pos.y - r, r*2, r*2}, targets);
                for (auto target : targets) {
                    if (target->type == ObjType::GLITCH_BASIC || target->type == ObjType::GLITCH_TANK || target->type == ObjType::GLITCH_DASH || target->type == ObjType::GLITCH_BOSS) {
                        uintptr_t tid = reinterpret_cast<uintptr_t>(target);
                        if (ray->hits.find(tid) == ray->hits.end() && ray->pos.dist(target->pos) < target->radius + ray->radius) {
                            Glitch* g = static_cast<Glitch*>(target);
                            g->stability -= ray->damage;
                            ray->hits.insert(tid);
                            spawn_particle_chaos(ray->pos, Colors::NEON_BLUE, 5);
                            apply_force_to_grid(ray->pos, 20.0f, 100.0f);
                            if (audio) audio->play_impact(); 
                            if (ray->pierce_left <= 0) ray->dead = true;
                            else ray->pierce_left--;

                            if (g->stability <= 0) {
                                g->dead = true;
                                spawn_particle_chaos(g->pos, g->color, (g->type == ObjType::GLITCH_BOSS) ? 150 : 25);
                                apply_force_to_grid(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 250.0f : 50.0f, 350.0f);
                                add_bloom(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 500.0f : 180.0f, g->color);
                                if (audio) {
                                    audio->play_randomized_sfx(528.0f, 0.15f, 0.25f, false, g->pos.x / (float)WINDOW_W);
                                    audio->play_restoration_sigh(); 
                                }
                                new_entities.push_back(std::make_unique<HarmonyOrb>(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 100.0f : 10.0f));
                            }
                            break;
                        }
                    }
                }
                if (!ray->dead && ray->life < dt) { if (audio) audio->play_dissipate(); }
            }

            if (e->type == ObjType::HARMONY_ORB) {
                float dist = e->pos.dist(player->pos);
                if (dist < 180.0f) e->vel = e->vel + (player->pos - e->pos).norm() * 1200.0f * dt;
                if (dist < player->radius + 15.0f) {
                    e->dead = true;
                    player->harmony_xp += static_cast<HarmonyOrb*>(e.get())->value;
                    if (audio) audio->play_collect_harmony(); 
                    if (player->harmony_xp >= player->harmony_next) {
                        player->harmony_xp = 0; player->harmony_next *= 1.25f; player->level++;
                        state = State::EVOLUTION;
                    }
                }
            }
        }
        
        for(auto& ne : new_entities) entities.push_back(std::move(ne));
        new_entities.clear();
        std::erase_if(entities, [](const auto& e) { return e->dead; });
    }

    void render() {
        SDL_SetRenderDrawColor(renderer, 10, 10, 35, 255); 
        SDL_RenderClear(renderer);
        int cols = (WINDOW_W / GRID_SIZE) + 1;
        gfx.set_color({70, 140, 255, 60});
        for(size_t i = 0; i < grid.size() ; ++i) {
            size_t x = i % cols; size_t y = i / cols;
            Vec2 p1 = grid[i].base + grid[i].offset;
            if (x < (size_t)cols - 1) gfx.draw_line(p1, grid[i+1].base + grid[i+1].offset);
            if (y < (size_t)(grid.size()/cols) - 1) gfx.draw_line(p1, grid[i+cols].base + grid[i+cols].offset);
        }
        for(const auto& p : pulses) gfx.draw_bloom(p.pos, p.radius, p.color);
        for(const auto& p : particles) {
            if(!p.active) continue;
            gfx.set_color(p.color, p.life); gfx.draw_circle(p.pos, p.radius * p.life);
        }
        if (state == State::PLAYING || state == State::EVOLUTION || state == State::GAME_OVER) {
            for (auto& e : entities) {
                if (e->type == ObjType::HARMONY_RAY) {
                    gfx.set_color(e->color, 0.9f);
                    Vec2 fwd = e->vel.norm() * 45.0f;
                    gfx.draw_line(e->pos, e->pos - fwd);
                    gfx.draw_glowing_circle(e->pos, e->radius, e->color);
                } else if (e->type != ObjType::PLAYER) {
                    gfx.draw_glowing_circle(e->pos, e->radius, e->color);
                    if (e->type == ObjType::GLITCH_BOSS) {
                        Glitch* g = static_cast<Glitch*>(e.get());
                        float pct = g->stability / g->max_stability;
                        gfx.set_color({80, 80, 80, 255});
                        gfx.draw_line(e->pos + Vec2(-60, -90), e->pos + Vec2(60, -90));
                        gfx.set_color(e->color);
                        gfx.draw_line(e->pos + Vec2(-60, -90), e->pos + Vec2(-60 + 120 * pct, -90));
                    }
                }
            }
            Vec2 dir(std::cos(player->angle), std::sin(player->angle));
            Vec2 nose = player->pos + dir * 24.0f;
            Vec2 l_wing = player->pos + dir * -14.0f + Vec2(-dir.y, dir.x) * 16.0f;
            Vec2 r_wing = player->pos + dir * -14.0f + Vec2(dir.y, -dir.x) * 16.0f;
            gfx.set_color(player->color);
            gfx.draw_line(nose, l_wing); gfx.draw_line(l_wing, r_wing); gfx.draw_line(r_wing, nose);
            gfx.draw_bloom(player->pos, 20.0f, player->color);
            gfx.draw_text(std::format("INTEGRITY: {:.0f}", player->integrity), {20, 20}, 26, {255, 200, 50, 255});
            gfx.draw_text(std::format("HARMONY: {:.0f}/{:.0f}", player->harmony_xp, player->harmony_next), {20, 55}, 22, Colors::NEON_GREEN);
            gfx.draw_text(std::format("EVOLUTION: {}", player->level), {20, 85}, 22, Colors::WHITE);
        }
        if (state == State::MENU) {
            gfx.draw_text_centered("NEON NEXUS: HARMONY", {WINDOW_W/2, 200}, 95, Colors::NEON_BLUE);
            gfx.draw_text_centered("RESTORE THE DIVINE BALANCE", {WINDOW_W/2, 350}, 32, Colors::WHITE);
            gfx.draw_text_centered("PRESS SPACE TO BEGIN", {WINDOW_W/2, 500}, 26, Colors::GOLD);
        } else if (state == State::EVOLUTION) {
            gfx.draw_text_centered("DIVINE EVOLUTION", {WINDOW_W/2, 150}, 75, Colors::NEON_GREEN);
            gfx.draw_text_centered("1: RAY DENSITY (+Spread)", {WINDOW_W/2, 350}, 30, Colors::WHITE);
            gfx.draw_text_centered("2: HARMONY POWER", {WINDOW_W/2, 450}, 30, Colors::WHITE);
            gfx.draw_text_centered("3: RESONANCE (+Piercing)", {WINDOW_W/2, 550}, 30, Colors::WHITE);
        } else if (state == State::GAME_OVER) {
            gfx.draw_text_centered("NEXUS DEFLATED", {WINDOW_W/2, 300}, 85, {255, 120, 50, 255});
            gfx.draw_text_centered("SPACE TO RE-HARMONIZE", {WINDOW_W/2, 450}, 32, Colors::WHITE);
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
                        if (ev.key.keysym.sym == SDLK_1) { 
                            player->ray_count++; player->ray_spread += 0.05f;
                            if(audio) audio->play_evolve_ray_density(); state = State::PLAYING; 
                        }
                        if (ev.key.keysym.sym == SDLK_2) { 
                            player->harmony_power *= 1.45f; 
                            if(audio) audio->play_evolve_harmony_power(); state = State::PLAYING; 
                        }
                        if (ev.key.keysym.sym == SDLK_3) { 
                            player->ray_pierce++; player->harmony_rate *= 0.9f; 
                            if(audio) audio->play_evolve_resonance_rate(); state = State::PLAYING; 
                        }
                    }
                }
            }
            update(dt);
            render();
        }
    }
};
