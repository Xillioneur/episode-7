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

struct TextParticle {
    Vec2 pos;
    std::string text;
    float life;
    Color color;
};

class Engine {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Graphics gfx;
    std::unique_ptr<AudioEngine> audio;
    WorkerPool workers{4};
    
    bool running = true;
    enum class State { MENU, PLAYING, EVOLUTION, GAME_OVER, VICTORY } state = State::MENU;

    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<GameObject>> entities;
    std::vector<std::unique_ptr<GameObject>> new_entities; 
    std::vector<BloomPulse> pulses;
    std::vector<TextParticle> text_particles;
    std::vector<BackgroundStar> background_stars;
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
    float bg_flash = 0.0f;

    int current_wave = 1;
    float wave_timer = 0.0f;
    int wave_spawn_count = 0;
    bool boss_active = false;

    int combo_count = 0;
    float combo_timer = 0.0f;

    // SACRED COVENANT SYSTEM (DOUAY-RHEIMS)
    std::vector<std::string> scriptures = {
        "THE LORD IS MY LIGHT AND MY SALVATION, WHOM SHALL I FEAR? - PSALM 26:1",
        "I CAN DO ALL THINGS IN HIM WHO STRENGTHENETH ME - PHILIPPIANS 4:13",
        "THE LIGHT SHINETH IN DARKNESS, AND THE DARKNESS DID NOT COMPREHEND IT - JOHN 1:5",
        "GOD IS OUR REFUGE AND STRENGTH: A HELPER IN TROUBLES - PSALM 45:2",
        "IF WE WALK IN THE LIGHT... WE HAVE FELLOWSHIP ONE WITH ANOTHER - 1 JOHN 1:7",
        "PEACE I LEAVE WITH YOU, MY PEACE I GIVE UNTO YOU - JOHN 14:27"
    };
    std::vector<std::string> divine_prose = {
        "CHARITY IS PATIENT, IS KIND: CHARITY ENVIETH NOT, DEALETH NOT PERVERSELY...",
        "FOR GOD SO LOVED THE WORLD, AS TO GIVE HIS ONLY BEGOTTEN SON...",
        "BEFORE I FORMED THEE IN THE BOWELS OF THE MOTHER, I KNEW THEE...",
        "GREATER LOVE THAN THIS NO MAN HATH, THAT A MAN LAY DOWN HIS LIFE FOR HIS FRIENDS.",
        "GOD IS LOVE: AND HE THAT ABIDETH IN LOVE, ABIDETH IN GOD, AND GOD IN HIM. - 1 JOHN 4:16",
        "KNOW YE THAT THE LORD HE IS GOD: HE MADE US, AND NOT WE OURSELVES. - PSALM 99:3",
        "THE CHARITY OF GOD IS POURED FORTH IN OUR HEARTS, BY THE HOLY GHOST. - ROMANS 5:5",
        "LO, CHILDREN ARE AN HERITAGE OF THE LORD: AND THE FRUIT OF THE WOMB IS HIS REWARD. - PSALM 126:3",
        "THOU HAST MADE US FOR THYSELF, O LORD, AND OUR HEART IS RESTLESS UNTIL IT RESTS IN THEE.",
        "NOT UNTO US, O LORD, NOT UNTO US, BUT UNTO THY NAME GIVE GLORY. - PSALM 113:9",
        "BEHOLD WHAT MANNER OF CHARITY THE FATHER HATH BESTOWED UPON US... - 1 JOHN 3:1",
        "IN ALL THINGS GIVE THANKS; FOR THIS IS THE WILL OF GOD IN CHRIST JESUS. - 1 THESS 5:18",
        "EVERY BEST GIFT, AND EVERY PERFECT GIFT, IS FROM ABOVE... - JAMES 1:17",
        "THY WORD IS A LAMP TO MY FEET, AND A LIGHT TO MY PATHS. - PSALM 118:105",
        "FAITH IS THE SUBSTANCE OF THINGS TO BE HOPED FOR, THE EVIDENCE OF THINGS THAT APPEAR NOT. - HEBREWS 11:1"
    };
    int current_scripture_idx = 0;
    float scripture_timer = 0.0f;
    float fruit_spawn_timer = 15.0f;
    bool messenger_open = false;
    float messenger_scroll = 0.0f;

    struct GridPoint { Vec2 base; Vec2 offset; Vec2 vel; };
    std::vector<GridPoint> grid;
    const int GRID_SIZE = 35;
    const int WINDOW_W = 1200;
    const int WINDOW_H = 800;

    Vec2 generate_spawn_pos() {
        static float angle = 0.0f;
        int pattern = (int)(stability_timer / 15.0f) % 5;
        if (pattern == 0) { 
            angle += 0.4f;
            float r = 900.0f;
            return Vec2(WINDOW_W/2 + std::cos(angle) * r, WINDOW_H/2 + std::sin(angle) * r);
        } else if (pattern == 1) { 
            static float tx = 0; tx += 0.5f;
            float x = WINDOW_W/2 + std::sin(tx) * WINDOW_W/2;
            float y = (rand() % 2) ? -60.0f : WINDOW_H + 60.0f;
            return Vec2(x, y);
        } else if (pattern == 2) {
            float a = rnd(0, std::numbers::pi_v<float>*2);
            return Vec2(WINDOW_W/2 + std::cos(a) * 800.0f, WINDOW_H/2 + std::sin(a) * 800.0f);
        } else if (pattern == 3) { // GRID SPAWN
            static int gx = 0; gx = (gx + 1) % 5;
            float x = (WINDOW_W / 4.0f) * gx;
            float y = (rand() % 2) ? -100.0f : WINDOW_H + 100.0f;
            return Vec2(x, y);
        } else { // SPIRAL INWARD
            static float sa = 0; sa += 0.2f;
            float sr = 1000.0f - (fmod(stability_timer, 15.0f) * 50.0f);
            return Vec2(WINDOW_W/2 + std::cos(sa) * sr, WINDOW_H/2 + std::sin(sa) * sr);
        }
    }

    void spawn_text_particle(Vec2 p, const std::string& txt, Color c) {
        text_particles.push_back({p, txt, 1.0f, c});
    }

    void render_messenger() {
        float bw = 1000, bh = 640;
        Vec2 center(WINDOW_W/2, WINDOW_H/2);
        float x = center.x - bw/2;
        float y = center.y - bh/2;
        
        gfx.draw_filled_rect(0, 0, WINDOW_W, WINDOW_H, {5, 5, 15, 200});
        gfx.draw_filled_rect(x, y, bw, bh, {10, 15, 35, 245});
        gfx.draw_rect(x, y, bw, bh, Colors::HOLY_GOLD);
        gfx.draw_rect(x + 5, y + 5, bw - 10, bh - 10, {255, 215, 0, 100});
        
        gfx.draw_text_centered("THE DIVINE ARCHIVE", {center.x, y + 60}, 42, Colors::HOLY_GOLD);
        gfx.draw_line({center.x - 200, y + 90}, {center.x + 200, y + 90});

        // SCROLL BAR
        float sb_x = x + bw - 25;
        float sb_y = y + 110;
        float sb_h = bh - 180;
        gfx.draw_filled_rect(sb_x, sb_y, 10, sb_h, {50, 50, 80, 100});
        
        float total_h = static_cast<float>(divine_prose.size()) * 120.0f;
        float handle_h = std::max(30.0f, (sb_h / total_h) * sb_h);
        float handle_y = sb_y + (messenger_scroll / std::max(1.0f, total_h - sb_h)) * (sb_h - handle_h);
        gfx.draw_filled_rect(sb_x, handle_y, 10, handle_h, Colors::HOLY_GOLD);

        // SCISSOR CLIPPING FOR SCROLLING
        SDL_Rect clip_rect = { static_cast<int>(x + 40), static_cast<int>(y + 110), static_cast<int>(bw - 100), static_cast<int>(bh - 180) };
        SDL_RenderSetClipRect(renderer, &clip_rect);

        for(int i = 0; i < (int)divine_prose.size(); ++i) {
            float line_y = y + 180 + i * 120.0f - messenger_scroll;
            if (line_y > y + 50 && line_y < y + bh - 50) {
                gfx.draw_text_centered_wrapped(divine_prose[i], {center.x + 2, line_y + 2}, 24, {0, 0, 0, 255}, bw - 150);
                gfx.draw_text_centered_wrapped(divine_prose[i], {center.x, line_y}, 24, Colors::DIVINE_WHITE, bw - 150);
            }
        }

        SDL_RenderSetClipRect(renderer, NULL); 
        
        float pulse = 0.7f + 0.3f * std::sin(stability_timer * 5.0f);
        gfx.draw_text_centered("SCROLL TO READ MORE - CLICK TO RETURN", {center.x, y + bh - 40}, 18, {255, 230, 100, (Uint8)(255 * pulse)});
    }

public:
    Engine() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        }
        audio = std::make_unique<AudioEngine>();
        if (!audio->start()) std::cerr << "Warning: Audio system offline." << std::endl;
        TTF_Init();
        window = SDL_CreateWindow("NEON NEXUS: THE DIVINE PATH", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
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
        
        for(int i=0; i<150; ++i) {
            Color star_c = (rand()%2==0) ? Colors::HOLY_GOLD : Colors::DIVINE_WHITE;
            background_stars.push_back({
                Vec2(rnd(0, WINDOW_W), rnd(0, WINDOW_H)),
                rnd(0.1f, 1.0f), 
                rnd(0.5f, 2.0f), 
                { star_c.r, star_c.g, star_c.b, (Uint8)rnd(50, 150) }
            });
        }

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
        text_particles.clear();
        for(auto& p : particles) p.active = false;
        player = std::make_unique<Player>(Vec2(WINDOW_W/2, WINDOW_H/2));
        glitch_spawn_timer = 0;
        stability_timer = 0;
        boss_spawn_timer = 60.0f;
        screenshake = 0;
        low_integrity_timer = 0;
        current_wave = 1;
        wave_timer = 0;
        wave_spawn_count = 0;
        boss_active = false;
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

    std::generator<Vec2> spawn_wave_generator(int count) {
        for (int i = 0; i < count; ++i) {
            co_yield generate_spawn_pos();
        }
    }

    void update(float dt) {
        float glitch_speed_mult = 1.0f;
        if (audio) {
            float x_norm = player->pos.x / (float)WINDOW_W;
            float speed_norm = player->vel.mag() / player->speed;
            float progress = player->harmony_xp / player->harmony_next;
            audio->set_player_state(x_norm, speed_norm, state == State::PLAYING, progress);
            
            auto m_state = audio->get_music_state();
            bg_flash = std::lerp(bg_flash, m_state.intensity * 0.15f, 0.1f);
            player->internal_pulse = 1.0f + m_state.intensity * 0.25f;
            glitch_speed_mult = 1.0f + m_state.intensity * 0.45f;
        }

        if (state != State::PLAYING) return;
        stability_timer += dt;
        
        if (screenshake > 0) screenshake -= dt * 30.0f;
        else screenshake = 0;
        bg_flash = std::max(0.0f, bg_flash - dt * 0.5f);
        if (combo_timer > 0) combo_timer -= dt;
        else combo_count = 0;

        scripture_timer += dt;
        if (scripture_timer > 10.0f) {
            scripture_timer = 0;
            current_scripture_idx = (current_scripture_idx + 1) % scriptures.size();
        }

        fruit_spawn_timer -= dt;
        if (fruit_spawn_timer <= 0) {
            fruit_spawn_timer = rnd(20.0f, 40.0f);
            static const std::pair<std::string, Color> fruits[] = {
                {"LOVE", {255, 50, 50, 255}}, {"JOY", Colors::HOLY_GOLD}, {"PEACE", Colors::RADIANT_TEAL}, {"PATIENCE", Colors::WHITE}
            };
            int idx = rand() % 4;
            new_entities.push_back(std::make_unique<FruitOfSpirit>(Vec2(rnd(100, WINDOW_W-100), -50), fruits[idx].first, fruits[idx].second));
        }

        if (player->blessing_timer > 0) player->blessing_timer -= dt;
        else player->active_virtue = "";

        float power_mult = (player->active_virtue == "LOVE") ? 2.0f : 1.0f;
        float speed_mult = (player->active_virtue == "JOY") ? 1.5f : 1.0f;
        player->siphon_range = (player->active_virtue == "PEACE") ? 500.0f : 250.0f;
        float enemy_slow = (player->active_virtue == "PATIENCE") ? 0.4f : 1.0f;

        gfx.cam_offset = Vec2(rnd(-screenshake, screenshake), rnd(-screenshake, screenshake));

        const Uint8* keys = SDL_GetKeyboardState(NULL);
        Vec2 move_input;
        if (keys[SDL_SCANCODE_W]) move_input.y -= 1;
        if (keys[SDL_SCANCODE_S]) move_input.y += 1;
        if (keys[SDL_SCANCODE_A]) move_input.x -= 1;
        if (keys[SDL_SCANCODE_D]) move_input.x += 1;
        if (move_input.mag() > 0) {
            Vec2 move_dir = move_input.norm();
            player->vel = player->vel + move_dir * player->speed * speed_mult * 8.0f * dt;
            apply_force_to_grid(player->pos, 12.0f, 120.0f);
        }

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        player->angle = std::atan2(static_cast<float>(my) - player->pos.y, static_cast<float>(mx) - player->pos.x);

        if (keys[SDL_SCANCODE_SPACE] && player->dash_cooldown <= 0) {
            player->vel = move_input.norm() * 1500.0f;
            player->dash_cooldown = 1.0f;
            screenshake = 15.0f;
            spawn_particle_chaos(player->pos, Colors::RADIANT_TEAL, 30);
            apply_force_to_grid(player->pos, 180.0f, 300.0f);
            add_bloom(player->pos, 250.0f, Colors::RADIANT_TEAL);
            if (audio) audio->play_warp_jump(); 
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
                new_entities.push_back(std::make_unique<HarmonyRay>(player->pos + dir * 20, dir * player->ray_speed, player->harmony_power * power_mult, Colors::DIVINE_WHITE, player->ray_pierce));
            }
        }

        player->update(dt);
        update_grid_parallel(dt);

        for(auto& s : background_stars) {
            s.pos = s.pos - player->vel * (dt * s.depth * 0.15f);
            if(s.pos.x < 0) s.pos.x += WINDOW_W;
            if(s.pos.x > WINDOW_W) s.pos.x -= WINDOW_W;
            if(s.pos.y < 0) s.pos.y += WINDOW_H;
            if(s.pos.y > WINDOW_H) s.pos.y -= WINDOW_H;
        }

        for(auto& p : particles) {
            if(!p.active) continue;
            p.pos = p.pos + p.vel * dt; p.life -= p.decay * dt; p.vel = p.vel * 0.96f;
            if(p.life <= 0) p.active = false;
        }

        for(auto& p : pulses) { p.radius += 600.0f * dt; p.life -= dt * 1.8f; }
        std::erase_if(pulses, [](const auto& p) { return p.life <= 0; });

        for(auto& tp : text_particles) { tp.pos.y -= 100.0f * dt; tp.life -= dt * 1.5f; }
        std::erase_if(text_particles, [](const auto& tp) { return tp.life <= 0; });

        glitch_spawn_timer -= dt;
        wave_timer += dt;

        int max_wave_spawns = 5 + current_wave * 3;
        float spawn_interval = 2.0f / (1.0f + current_wave * 0.2f);

        if (glitch_spawn_timer <= 0 && wave_spawn_count < max_wave_spawns) {
            float difficulty = current_wave + (stability_timer / 60.0f);
            glitch_spawn_timer = spawn_interval;
            for (auto spawn_pos : spawn_wave_generator(1)) {
                ObjType t = ObjType::GLITCH_BASIC;
                if (current_wave >= 2 && rand()%4==0) t = ObjType::GLITCH_DASH;
                if (current_wave >= 3 && rand()%5==0) t = ObjType::GLITCH_SPLITTER;
                if (current_wave >= 4 && rand()%5==0) t = ObjType::GLITCH_TANK;
                auto glitch_res = create_glitch(spawn_pos, t, difficulty);
                if (glitch_res) { new_entities.push_back(std::move(*glitch_res)); apply_force_to_grid(spawn_pos, 60.0f, 180.0f); wave_spawn_count++; }
            }
        }

        bool entities_clear = std::none_of(entities.begin(), entities.end(), [](const auto& e) {
            return e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_SPLITTER;
        });

        if (wave_spawn_count >= max_wave_spawns && entities_clear && !boss_active) {
            Vec2 spawn_pos = generate_spawn_pos();
            auto boss_res = create_glitch(spawn_pos, ObjType::GLITCH_BOSS, current_wave * 0.5f);
            if (boss_res) { new_entities.push_back(std::move(*boss_res)); add_bloom(spawn_pos, 600.0f, Colors::HOLY_GOLD); apply_force_to_grid(spawn_pos, 400.0f, 700.0f); if (audio) audio->play_glitch_spawn(); screenshake = 30.0f; boss_active = true; }
        }

        bool boss_exists = std::any_of(entities.begin(), entities.end(), [](const auto& e) { return e->type == ObjType::GLITCH_BOSS; });
        if (boss_active && !boss_exists) { current_wave++; wave_spawn_count = 0; boss_active = false; if (audio) audio->play_restoration_sigh(); }

        if (player->level >= 10) state = State::VICTORY;

        qtree->clear();
        for (auto& e : entities) { if (e->type != ObjType::PARTICLE) qtree->insert(e.get()); }

        for (auto& e : entities) {
            e->update(dt);
            if (e->dead) continue;
            if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_BOSS || e->type == ObjType::GLITCH_SPLITTER) {
                Glitch* g = static_cast<Glitch*>(e.get());
                Vec2 dir = (player->pos - e->pos).norm();
                if (e->type == ObjType::GLITCH_DASH) {
                    if (g->internal_state == 0) { e->vel = e->vel + dir * 150.0f * dt * glitch_speed_mult * enemy_slow; if (g->state_timer <= 0) { g->internal_state = 1; g->state_timer = 0.8f; } }
                    else if (g->internal_state == 1) { e->vel = e->vel * 0.9f; if (g->state_timer <= 0) { g->internal_state = 2; g->state_timer = 0.4f; e->vel = dir * 1200.0f * glitch_speed_mult * enemy_slow; } }
                    else if (g->internal_state == 2) { if (g->state_timer <= 0) { g->internal_state = 0; g->state_timer = 2.0f; } }
                } else if (e->type == ObjType::GLITCH_BOSS) {
                    e->vel = e->vel + dir * 45.0f * dt * 2.2f * glitch_speed_mult * enemy_slow;
                    if (g->state_timer <= 0) { for(int i=0; i<8; ++i) { float a = i * (std::numbers::pi_v<float> * 2.0f / 8.0f); Vec2 pdir(std::cos(a), std::sin(a)); new_entities.push_back(std::make_unique<GlitchProjectile>(e->pos + pdir * 70.0f, pdir * 300.0f * enemy_slow)); } g->state_timer = 3.0f; }
                } else if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_SPLITTER) {
                    if (g->state_timer <= 0) { float roll = rnd(0, 1); if (g->internal_state == 0) g->internal_state = (roll < 0.7f) ? 1 : 2; else if (g->internal_state == 1) g->internal_state = (roll < 0.5f) ? 0 : 2; else g->internal_state = (roll < 0.6f) ? 0 : 1; g->state_timer = rnd(0.8f, 2.5f); }
                    if (g->internal_state == 0) e->vel = e->vel + dir * 250.0f * dt * glitch_speed_mult * enemy_slow; else if (g->internal_state == 1) e->vel = e->vel + dir * 600.0f * dt * glitch_speed_mult * enemy_slow; else { Vec2 side(-dir.y, dir.x); e->vel = e->vel + (dir * 0.8f + side) * 350.0f * dt * glitch_speed_mult * enemy_slow; }
                } else if (e->type == ObjType::GLITCH_TANK) {
                    e->vel = e->vel + dir * 55.0f * dt * 2.2f * glitch_speed_mult * enemy_slow;
                    std::vector<GameObject*> nearby; qtree->query(Rect{e->pos.x - 150, e->pos.y - 150, 300, 300}, nearby);
                    for (auto n : nearby) { if (n != e.get() && (n->type == ObjType::GLITCH_BASIC || n->type == ObjType::GLITCH_DASH || n->type == ObjType::GLITCH_SPLITTER)) { static_cast<Glitch*>(n)->stability = std::min(static_cast<Glitch*>(n)->max_stability, static_cast<Glitch*>(n)->stability + 8.0f * dt); } }
                }
                e->vel = e->vel * 0.95f;
                if (e->pos.dist(player->pos) < e->radius + player->radius) { player->integrity -= (e->type == ObjType::GLITCH_BOSS) ? 35 : 20; screenshake = (e->type == ObjType::GLITCH_BOSS) ? 35.0f : 15.0f; hit_stop = 0.1f; spawn_particle_chaos(player->pos, Colors::GOLD, 40); apply_force_to_grid(player->pos, 100.0f, 250.0f); if (audio) audio->play_player_damage(); e->dead = (e->type != ObjType::GLITCH_BOSS); if (player->integrity <= 0) state = State::GAME_OVER; }
            }
            if (e->type == ObjType::GLITCH_PROJECTILE) { if (e->pos.dist(player->pos) < e->radius + player->radius) { player->integrity -= 5; screenshake = 8.0f; spawn_particle_chaos(player->pos, Colors::RED, 15); if (audio) audio->play_player_damage(); e->dead = true; if (player->integrity <= 0) state = State::GAME_OVER; } }
            if (e->type == ObjType::HARMONY_RAY) {
                HarmonyRay* ray = static_cast<HarmonyRay*>(e.get()); float r = ray->radius + 30.0f; std::vector<GameObject*> targets; qtree->query(Rect{ray->pos.x - r, ray->pos.y - r, r*2, r*2}, targets);
                for (auto target : targets) {
                    if (target->type == ObjType::GLITCH_BASIC || target->type == ObjType::GLITCH_TANK || target->type == ObjType::GLITCH_DASH || target->type == ObjType::GLITCH_BOSS || target->type == ObjType::GLITCH_SPLITTER) {
                        uintptr_t tid = reinterpret_cast<uintptr_t>(target);
                        if (ray->hits.find(tid) == ray->hits.end() && ray->pos.dist(target->pos) < target->radius + ray->radius) {
                            Glitch* g = static_cast<Glitch*>(target); g->stability -= ray->damage; g->hit_timer = 0.15f; ray->hits.insert(tid); spawn_particle_chaos(ray->pos, Colors::DIVINE_WHITE, 5); apply_force_to_grid(ray->pos, 20.0f, 100.0f); if (audio) audio->play_impact(); 
                            if (ray->pierce_left <= 0) ray->dead = true; else ray->pierce_left--;
                            if (g->stability <= 0) {
                                g->dead = true; combo_count++; combo_timer = 1.5f; spawn_particle_chaos(g->pos, Colors::GOLD, (g->type == ObjType::GLITCH_BOSS) ? 150 : 25);
                                static const std::string notes[] = {"#", "b", "♪", "*"}; for(int i=0; i<3; ++i) { spawn_text_particle(g->pos + Vec2(rnd(-20, 20), rnd(-20, 20)), notes[rand()%4], Colors::WHITE); }
                                apply_force_to_grid(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 250.0f : 80.0f, 400.0f); add_bloom(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 500.0f : 180.0f, g->color);
                                if (audio) { audio->play_harmonic_combo(combo_count); if (g->type == ObjType::GLITCH_BOSS) audio->play_restoration_sigh(); }
                                if (g->type == ObjType::GLITCH_SPLITTER) { for(int i=0; i<3; ++i) { auto mini = create_glitch(g->pos + Vec2(rnd(-15, 15), rnd(-15, 15)), ObjType::GLITCH_BASIC, current_wave * 0.5f); if (mini) { (*mini)->radius = 8.0f; new_entities.push_back(std::move(*mini)); } } }
                                new_entities.push_back(std::make_unique<HarmonyOrb>(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 100.0f : 10.0f));
                            }
                            break;
                        }
                    }
                }
                if (!ray->dead && ray->life < dt) { if (audio) audio->play_dissipate(); }
            }
        }

        std::vector<GameObject*> nearby_orbs; qtree->query(Rect{player->pos.x - 300, player->pos.y - 300, 600, 600}, nearby_orbs);
        int siphoning_count = 0;
        for (auto orb_ptr : nearby_orbs) {
            if (orb_ptr->type == ObjType::HARMONY_ORB) {
                float dist = orb_ptr->pos.dist(player->pos);
                if (dist < player->siphon_range) { float pull_force = 1500.0f * (1.0f - dist / player->siphon_range); orb_ptr->vel = orb_ptr->vel + (player->pos - orb_ptr->pos).norm() * pull_force * dt; siphoning_count++; gfx.set_color(Colors::HOLY_GOLD, 0.3f); gfx.draw_line(orb_ptr->pos, player->pos); }
                if (dist < player->radius + 15.0f) { orb_ptr->dead = true; player->harmony_xp += static_cast<HarmonyOrb*>(orb_ptr)->value; if (audio) audio->play_collect_harmony(); if (player->harmony_xp >= player->harmony_next) { player->harmony_xp = 0; player->harmony_next *= 1.25f; player->level++; state = State::EVOLUTION; } }
            }
        }
        if (siphoning_count > 0) player->integrity = std::min(player->max_integrity, player->integrity + siphoning_count * 0.5f * dt);
        for(auto& ne : new_entities) entities.push_back(std::move(ne));
        new_entities.clear(); std::erase_if(entities, [](const auto& e) { return e->dead; });
    }

    void render() {
        Color bg = Colors::BG_DARK;
        SDL_SetRenderDrawColor(renderer, (Uint8)(bg.r + bg_flash * 100), (Uint8)(bg.g + bg_flash * 150), (Uint8)(bg.b + bg_flash * 255), 255); SDL_RenderClear(renderer);
        for(const auto& s : background_stars) { float pulse = 1.0f + bg_flash * 3.0f; gfx.set_color(s.color, std::min(1.0f, (s.color.a / 255.0f) * pulse)); gfx.draw_circle(s.pos, s.size * pulse); }
        int cols = (WINDOW_W / GRID_SIZE) + 1; int movement = 0; if (audio) movement = audio->get_music_state().movement;
        Color base_grid = {70, 140, 255, 30}; if (movement == 1) base_grid = {255, 100, 200, 30}; else if (movement == 2) base_grid = {255, 230, 100, 30}; else if (movement == 3) base_grid = {100, 255, 200, 30}; else if (movement == 4) base_grid = {200, 100, 255, 30}; 
        for(size_t i = 0; i < grid.size() ; ++i) {
            size_t x = i % cols; size_t y = i / cols; Vec2 p1 = grid[i].base + grid[i].offset; float dist_sq = grid[i].offset.x * grid[i].offset.x + grid[i].offset.y * grid[i].offset.y; float intensity = std::min(1.0f, dist_sq / 400.0f); float d_player = p1.dist(player->pos); float light = std::max(0.0f, 1.0f - d_player / 150.0f); Color grid_c = lerp_color(base_grid, {255, 255, 255, 220}, std::max(intensity, light)); gfx.set_color(grid_c);
            if (x < (size_t)cols - 1) gfx.draw_line(p1, grid[i+1].base + grid[i+1].offset); if (y < (size_t)(grid.size()/cols) - 1) gfx.draw_line(p1, grid[i+cols].base + grid[i+cols].offset);
        }
        for(const auto& p : pulses) gfx.draw_bloom(p.pos, p.radius, p.color);
        for(const auto& tp : text_particles) { gfx.draw_text(tp.text, tp.pos, 24, {tp.color.r, tp.color.g, tp.color.b, (Uint8)(255 * tp.life)}); }
        for(const auto& p : particles) { if(!p.active) continue; gfx.set_color(p.color, p.life); float s = p.radius * p.life; gfx.draw_line(p.pos - Vec2(s, 0), p.pos + Vec2(s, 0)); gfx.draw_line(p.pos - Vec2(0, s), p.pos + Vec2(0, s)); }
        
        // --- HUD POLISH (DIVINE MODERN) ---
        if (state == State::PLAYING || state == State::EVOLUTION || state == State::GAME_OVER || state == State::VICTORY) {
            // GRACE ORB (Top Left)
            float grace_pct = player->integrity / player->max_integrity;
            Color grace_c = lerp_color(Colors::RED, Colors::HOLY_GOLD, grace_pct);
            gfx.draw_glowing_circle({60, 60}, 30.0f * (0.8f + 0.2f * std::sin(stability_timer * 10.0f)), grace_c);
            gfx.draw_text(std::format("{:.0f}%", player->integrity), {45, 50}, 20, Colors::WHITE);
            gfx.draw_text("GRACE", {40, 100}, 18, Colors::HOLY_GOLD);

            // MESSENGER ICON (Top Right)
            gfx.draw_cross({static_cast<float>(WINDOW_W - 60), 60}, 20, Colors::HOLY_GOLD, 3.0f);
            gfx.draw_bloom({static_cast<float>(WINDOW_W - 60), 60}, 30, Colors::HOLY_GOLD);
            gfx.draw_text("WISDOM", {static_cast<float>(WINDOW_W - 90), 100}, 18, Colors::HOLY_GOLD);

            // FAITH BAR (Bottom Center)
            float xp_pct = player->harmony_xp / player->harmony_next;
            gfx.draw_filled_rect(WINDOW_W/2 - 200, WINDOW_H - 80, 400, 6, {50, 50, 50, 150});
            gfx.draw_filled_rect(WINDOW_W/2 - 200, WINDOW_H - 80, 400 * xp_pct, 6, Colors::HOLY_GOLD);
            gfx.draw_text_centered(std::format("FAITH LEVEL {}", player->level), {static_cast<float>(WINDOW_W/2), static_cast<float>(WINDOW_H - 100)}, 18, Colors::WHITE);

            for (auto& e : entities) {
                if (e->type == ObjType::HARMONY_RAY) {
                    Vec2 fwd = e->vel.norm(); for(int i=0; i<12; ++i) { float t = i / 12.0f; gfx.set_color(e->color, (1.0f - t) * 0.5f); gfx.draw_line(e->pos - fwd * (i * 5.0f), e->pos - fwd * ((i+1) * 5.0f)); gfx.set_color(Colors::WHITE, (1.0f - t) * 0.8f); gfx.draw_line(e->pos - fwd * (i * 3.0f), e->pos - fwd * ((i+1) * 3.0f)); }
                    if (rand()%2==0) spawn_particle_chaos(e->pos - fwd * rnd(0, 40), Colors::WHITE, 1, 50.0f); gfx.draw_glowing_circle(e->pos, e->radius, e->color);
                } else if (e->type != ObjType::PLAYER && e->type != ObjType::HARMONY_ORB) {
                    if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_BOSS || e->type == ObjType::GLITCH_SPLITTER) {
                        Glitch* g = static_cast<Glitch*>(e.get()); float hit_scale = 1.0f + (g->hit_timer > 0 ? g->hit_timer * 2.0f : 0.0f); Color draw_c = g->hit_timer > 0 ? Colors::WHITE : g->color;
                        if (e->type == ObjType::GLITCH_DASH) { if (g->internal_state == 1) { if ((int)(g->state_timer * 20) % 2 == 0) draw_c = Colors::WHITE; } else if (g->internal_state == 2) { for(int i=1; i<4; ++i) gfx.draw_wireframe_3d(g->pos - e->vel * (i * 0.015f), g->vertices, g->edges, g->rx, g->ry, g->rz, g->radius * (1.0f - i*0.2f), {draw_c.r, draw_c.g, draw_c.b, (Uint8)(100/i)}); } }
                        gfx.draw_wireframe_3d(g->pos, g->vertices, g->edges, g->rx, g->ry, g->rz, g->radius * hit_scale, draw_c);
                        if (e->type == ObjType::GLITCH_TANK) { gfx.draw_wireframe_3d(g->pos, g->vertices, g->edges, -g->rx, -g->ry, -g->rz, g->aura_radius, {g->color.r, g->color.g, g->color.b, 40}); }
                        if (e->type == ObjType::GLITCH_BOSS) { float pct = g->stability / g->max_stability; gfx.set_color({80, 80, 80, 255}); gfx.draw_line(e->pos + Vec2(-60, -90), e->pos + Vec2(60, -90)); gfx.set_color(e->color); gfx.draw_line(e->pos + Vec2(-60, -90), e->pos + Vec2(-60 + 120 * pct, -90)); }
                    } else { gfx.draw_glowing_circle(e->pos, e->radius, e->color); }
                } else if (e->type == ObjType::HARMONY_ORB) { gfx.draw_glowing_circle(e->pos, e->radius, e->color); }
            }
            static const std::vector<Vec3> ship_verts = { {1.2, 0, 0}, {0.4, -0.4, -0.3}, {0.4, 0.4, -0.3}, {-0.2, -0.6, 0}, {-0.2, 0.6, 0}, {-1.0, -0.7, 0.2}, {-1.0, 0.7, 0.2}, {-0.4, -1.0, 0.4}, {-0.4, 1.0, 0.4} };
            static const std::vector<std::pair<int, int>> ship_edges = { {0,1}, {0,2}, {1,2}, {1,3}, {2,4}, {3,4}, {3,5}, {4,6}, {5,6}, {3,7}, {4,8} };
            float roll = player->vel.x * 0.002f, pitch = -player->vel.y * 0.002f;
            for (const auto& tp : player->trail) gfx.draw_wireframe_3d(tp.pos, ship_verts, ship_edges, pitch, roll, tp.angle, 18.0f * tp.life, {player->color.r, player->color.g, player->color.b, (Uint8)(100 * tp.life)});
            gfx.draw_wireframe_3d(player->pos, ship_verts, ship_edges, pitch, roll, player->angle, 22.0f * player->internal_pulse, player->color);
            float pulse = 0.8f + 0.2f * std::sin(stability_timer * 18.0f);
            gfx.draw_glowing_circle(player->pos, 5.0f * pulse, Colors::WHITE); gfx.draw_bloom(player->pos, 12.0f * pulse, player->color);
            Vec2 pad_dir(std::cos(player->angle), std::sin(player->angle)), pad_side(-pad_dir.y, pad_dir.x);
            Vec2 pad_l = player->pos - pad_dir * 15.0f - pad_side * 10.0f, pad_r = player->pos - pad_dir * 15.0f + pad_side * 10.0f;
            float speed_factor = player->vel.mag() / player->speed, hover_pulse = 0.7f + 0.3f * std::sin(stability_timer * 12.0f);
            gfx.draw_bloom(player->pos, 35.0f * hover_pulse, {player->color.r, player->color.g, player->color.b, 40});
            for (int i = 0; i < 3; ++i) { float rs = std::fmod((stability_timer * 2.0f) + (i * 0.33f), 1.0f), ra = 1.0f - rs, rr = 5.0f + rs * (15.0f + speed_factor * 20.0f); gfx.set_color(Colors::HOLY_GOLD, ra * 0.6f); gfx.draw_circle(pad_l, rr); gfx.draw_circle(pad_r, rr); }
            float pi = 0.4f + speed_factor * 0.6f; gfx.set_color(Colors::WHITE, pi); gfx.draw_circle(pad_l, 4.0f); gfx.draw_circle(pad_r, 4.0f); gfx.draw_bloom(pad_l, 10.0f * pi, Colors::HOLY_GOLD); gfx.draw_bloom(pad_r, 10.0f * pi, Colors::HOLY_GOLD);
            
            if (combo_count > 1) { std::string title = combo_count > 10 ? "DIVINE GLORY" : (combo_count > 5 ? "HOLY HARMONY" : "RESONANCE"); gfx.draw_text(std::format("{} x{}", title, combo_count), {static_cast<float>(WINDOW_W/2 - 100), 70}, 32, Colors::HOLY_GOLD); }
            if (!player->active_virtue.empty()) { gfx.draw_text(std::format("BLESSING: {}", player->active_virtue), {static_cast<float>(WINDOW_W/2 - 80), 120}, 24, Colors::WHITE); gfx.set_color(Colors::HOLY_GOLD, player->blessing_timer / 8.0f); gfx.draw_line({static_cast<float>(WINDOW_W/2 - 100), 150}, {static_cast<float>(WINDOW_W/2 + 100), 150}); }
        }

        if (state == State::MENU) {
            gfx.draw_text_centered("NEON NEXUS: THE DIVINE PATH", {static_cast<float>(WINDOW_W/2), 200}, 85, Colors::HOLY_GOLD);
            gfx.draw_text_centered("SPREAD THE LIGHT, RESTORE THE BALANCE", {static_cast<float>(WINDOW_W/2), 350}, 28, Colors::WHITE);
            gfx.draw_text_centered("PRESS SPACE TO BEGIN YOUR JOURNEY", {static_cast<float>(WINDOW_W/2), 500}, 24, Colors::HOLY_GOLD);
        } else if (state == State::EVOLUTION) {
            gfx.draw_filled_rect(0, 0, WINDOW_W, WINDOW_H, {5, 5, 15, 180});
            gfx.draw_text_centered("ASCEND IN VIRTUE", {static_cast<float>(WINDOW_W/2), 120}, 75, Colors::HOLY_GOLD);
            gfx.draw_text_centered(scriptures[current_scripture_idx], {static_cast<float>(WINDOW_W/2), 200}, 22, Colors::WHITE);
            
            // CARD LAYOUT
            for(int i=0; i<3; ++i) {
                float cx = WINDOW_W/4.0f * (i+1);
                gfx.draw_filled_rect(cx - 130, 300, 260, 300, {20, 25, 50, 220});
                gfx.draw_rect(cx - 130, 300, 260, 300, Colors::HOLY_GOLD);
                gfx.draw_text_centered(std::format("{}", i+1), {cx, 340}, 45, Colors::HOLY_GOLD);
            }
            gfx.draw_text_centered("MULTIPLYING", {WINDOW_W/4.0f, 420}, 24, Colors::WHITE);
            gfx.draw_text_centered("BLESSINGS", {WINDOW_W/4.0f, 450}, 24, Colors::WHITE);
            gfx.draw_text_centered("MIGHT OF", {WINDOW_W/2.0f, 420}, 24, Colors::WHITE);
            gfx.draw_text_centered("THE WORD", {WINDOW_W/2.0f, 450}, 24, Colors::WHITE);
            gfx.draw_text_centered("STEADFAST", {WINDOW_W*0.75f, 420}, 24, Colors::WHITE);
            gfx.draw_text_centered("FAITH", {WINDOW_W*0.75f, 450}, 24, Colors::WHITE);
        } else if (state == State::GAME_OVER) {
            gfx.draw_text_centered("SPIRITUAL DISSONANCE", {static_cast<float>(WINDOW_W/2), 300}, 85, Colors::RED);
            gfx.draw_text_centered("SPACE TO RE-ALIGN WITH GRACE", {static_cast<float>(WINDOW_W/2), 450}, 32, Colors::WHITE);
        } else if (state == State::VICTORY) {
            gfx.draw_text_centered("DIVINE BALANCE RESTORED", {static_cast<float>(WINDOW_W/2), 250}, 75, Colors::HOLY_GOLD);
            gfx.draw_text_centered("THE NEXUS IS SANCTIFIED", {static_cast<float>(WINDOW_W/2), 380}, 32, Colors::WHITE);
            gfx.draw_text_centered("SPACE TO ASCEND AGAIN", {static_cast<float>(WINDOW_W/2), 550}, 26, Colors::HOLY_GOLD);
        }

        if (messenger_open) render_messenger();
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
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    if (messenger_open) messenger_open = false;
                    else {
                        int mx, my; SDL_GetMouseState(&mx, &my);
                        if (mx > WINDOW_W - 100 && my < 100) messenger_open = true;
                    }
                }
                if (ev.type == SDL_MOUSEWHEEL && messenger_open) {
                    messenger_scroll -= ev.wheel.y * 40.0f;
                    float max_scroll = std::max(0.0f, (static_cast<float>(divine_prose.size()) * 120.0f) - 300.0f);
                    messenger_scroll = std::clamp(messenger_scroll, 0.0f, max_scroll);
                }
                if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) running = false;
                    if (state == State::MENU && ev.key.keysym.sym == SDLK_SPACE) state = State::PLAYING;
                    if ((state == State::GAME_OVER || state == State::VICTORY) && ev.key.keysym.sym == SDLK_SPACE) { reset_game(); state = State::PLAYING; }
                    if (state == State::EVOLUTION) {
                        if (ev.key.keysym.sym == SDLK_1) { player->ray_count++; player->ray_spread += 0.05f; if(audio) audio->play_evolve_ray_density(); state = State::PLAYING; }
                        if (ev.key.keysym.sym == SDLK_2) { player->harmony_power *= 1.45f; if(audio) audio->play_evolve_harmony_power(); state = State::PLAYING; }
                        if (ev.key.keysym.sym == SDLK_3) { player->ray_pierce++; player->harmony_rate *= 0.9f; if(audio) audio->play_evolve_resonance_rate(); state = State::PLAYING; }
                    }
                }
            }
            update(dt);
            render();
        }
    }
};