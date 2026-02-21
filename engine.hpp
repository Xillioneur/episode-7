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

struct DesertSand {
    Vec2 pos;
    float size;
    float phase;
    float speed;
};

class Engine {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Graphics gfx;
    std::unique_ptr<AudioEngine> audio;
    WorkerPool workers{4};
    
    bool running = true;
    enum class State { MENU, PLAYING, EVOLUTION, GAME_OVER, VICTORY, PAUSED } state = State::MENU;

    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<GameObject>> entities;
    std::vector<std::unique_ptr<GameObject>> new_entities; 
    std::vector<BloomPulse> pulses;
    std::vector<TextParticle> text_particles;
    std::vector<BackgroundStar> background_stars;
    std::vector<DesertSand> desert_sands;
    std::unique_ptr<QuadTree> qtree;
    
    static constexpr int MAX_PARTICLES = 2000;
    std::array<ParticleData, MAX_PARTICLES> particles;
    int particle_ptr = 0;
    
    float glitch_spawn_timer = 0.0f;
    float stability_timer = 0.0f;
    float screenshake = 0.0f;
    float hit_stop = 0.0f;
    float bg_flash = 0.0f;

    int current_wave = 1;
    float wave_timer = 0.0f;
    int wave_spawn_count = 0;
    bool boss_active = false;
    
    int score = 0;
    int high_score = 0;

    int combo_count = 0;
    float combo_timer = 0.0f;

    // SACRED LENTEN MESSENGER SYSTEM
    std::string current_divine_msg = "";
    float divine_msg_timer = 0.0f;
    std::vector<std::string> idle_lenten_quotes = {
        "REMEMBER THAT YOU ARE DUST, AND TO DUST YOU SHALL RETURN.",
        "MAN DOES NOT LIVE BY BREAD ALONE.",
        "REND YOUR HEARTS, NOT YOUR GARMENTS.",
        "A CLEAN HEART CREATE FOR ME, O GOD.",
        "BE STILL AND KNOW THAT I AM GOD.",
        "THE DESERT SHALL REJOICE AND BLOSSOM."
    };

    struct Dialogue { std::string msg; std::vector<std::string> replies; };
    std::vector<Dialogue> active_dialogues = {
        {"HOW IS THY VIGIL, CHILD OF LIGHT?", {"IT IS WELL.", "THE DESERT IS VAST.", "I SEEK RESONANCE."}},
        {"THE SHADOWS FLEE BEFORE THY RAYS.", {"GLORY TO THE SOURCE.", "THEY ARE BUT DUST.", "I SHALL NOT FEAR."}},
        {"THINE INTEGRITY SHINES BRIGHTLY.", {"BY GRACE ALONE.", "I STRIVE FOR HARMONY.", "THE CODE IS STRONG."}},
        {"WATCH AND PRAY, LEST YE FALL.", {"I AM VIGILANT.", "MY CORE IS READY.", "STRENGTHEN ME."}},
        {"THE LIGHT PENETRATES THE VOID.", {"IT IS BEAUTIFUL.", "RESTORATION COMES.", "I AM THE VESSEL."}}
    };
    int current_dialog_idx = -1;
    int selected_reply = 0;
    float dialog_timer = 0.0f;

    bool evolution_pending = false;
    float evolution_timer = 0.0f;

    // SACRED COVENANT SYSTEM (DOUAY-RHEIMS - LENT FOCUS)
    std::vector<std::string> scriptures = {
        "NOT IN BREAD ALONE DOTH MAN LIVE - MATTHEW 4:4",
        "BE THOU FAITHFUL UNTO DEATH - APOCALYPSE 2:10",
        "THY WORD IS A LAMP TO MY FEET - PSALM 118:105",
        "GOD IS FAITHFUL, WHO WILL NOT SUFFER YOU TO BE TEMPTED ABOVE THAT WHICH YOU ARE ABLE - 1 COR 10:13",
        "HAVE MERCY ON ME, O GOD, ACCORDING TO THY GREAT MERCY - PSALM 50:1",
        "WATCH YE, AND PRAY THAT YE ENTER NOT INTO TEMPTATION - MATTHEW 26:41"
    };
    std::vector<std::string> divine_prose = {
        "AS THE HART PANTETH AFTER THE FOUNTAINS OF WATER; SO MY SOUL PANTETH AFTER THEE, O GOD.",
        "FOR FORTY DAYS AND FORTY NIGHTS, THE LIGHT PREPARED IN THE SILENCE OF THE VOID.",
        "CHARITY COVERETH A MULTITUDE OF SINS. GIVE ALMS OF LIGHT TO THE BROKEN SHADOWS.",
        "WHOSOEVER SHALL EXALT HIMSELF SHALL BE HUMBLED: AND HE THAT SHALL HUMBLE HIMSELF SHALL BE EXALTED.",
        "THE SACRIFICE OF GOD IS AN AFFLICTED SPIRIT: A CONTRITE AND HUMBLED HEART, O GOD, THOU WILT NOT DESPISE.",
        "I AM THE RESURRECTION AND THE LIFE: HE THAT BELIEVETH IN ME, ALTHOUGH HE BE DEAD, SHALL LIVE."
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
        if (pattern == 0) { angle += 0.4f; float r = 900.0f; return Vec2(WINDOW_W/2 + std::cos(angle) * r, WINDOW_H/2 + std::sin(angle) * r); }
        else if (pattern == 1) { static float tx = 0; tx += 0.5f; float x = WINDOW_W/2 + std::sin(tx) * WINDOW_W/2; float y = (rand() % 2) ? -60.0f : WINDOW_H + 60.0f; return Vec2(x, y); }
        else if (pattern == 2) { float a = rnd(0, std::numbers::pi_v<float>*2); return Vec2(WINDOW_W/2 + std::cos(a) * 800.0f, WINDOW_H/2 + std::sin(a) * 800.0f); }
        else if (pattern == 3) { static int gx = 0; gx = (gx + 1) % 5; float x = (WINDOW_W / 4.0f) * gx; float y = (rand() % 2) ? -100.0f : WINDOW_H + 100.0f; return Vec2(x, y); }
        else { static float sa = 0; sa += 0.2f; float sr = 1000.0f - (fmod(stability_timer, 15.0f) * 50.0f); return Vec2(WINDOW_W/2 + std::cos(sa) * sr, WINDOW_H/2 + std::sin(sa) * sr); }
    }

    void trigger_divine_voice(const std::string& msg) { current_divine_msg = msg; divine_msg_timer = 5.0f; }
    void trigger_dialogue() {
        if (current_dialog_idx == -1) {
            current_dialog_idx = rand() % active_dialogues.size();
            selected_reply = 0;
            dialog_timer = 15.0f; // Dialogue stays for 15 seconds
        }
    }
    void spawn_text_particle(Vec2 p, const std::string& txt, Color c) { text_particles.push_back({p, txt, 1.0f, c}); }

    void render_player_bubble() {
        if (current_dialog_idx == -1) return;
        auto& d = active_dialogues[current_dialog_idx];
        
        // Entrance animation scale
        float entrance = std::min(1.0f, (15.0f - dialog_timer) * 4.0f);
        float exit = std::min(1.0f, dialog_timer * 2.0f);
        float scale = entrance * exit;
        if (scale <= 0) return;

        // Messenger Prompt (Top Center)
        float my = 120.0f;
        gfx.draw_filled_rect(WINDOW_W/2 - 300, my, 600, 60, {10, 10, 30, (Uint8)(200 * scale)});
        gfx.draw_rect(WINDOW_W/2 - 300, my, 600, 60, {Colors::RESONANCE_GOLD.r, Colors::RESONANCE_GOLD.g, Colors::RESONANCE_GOLD.b, (Uint8)(255 * scale)});
        gfx.draw_text_centered(d.msg, {WINDOW_W/2.0f, my + 30}, 20, {Colors::NEXUS_WHITE.r, Colors::NEXUS_WHITE.g, Colors::NEXUS_WHITE.b, (Uint8)(255 * scale)});

        // Player Bubble (Follows player)
        Vec2 bp = player->pos + Vec2(40, -120);
        float bw = 240, bh = 140;
        gfx.draw_filled_rect(bp.x, bp.y, bw * scale, bh * scale, {20, 20, 40, (Uint8)(230 * scale)});
        gfx.draw_rect(bp.x, bp.y, bw * scale, bh * scale, {Colors::HARMONY_GOLD.r, Colors::HARMONY_GOLD.g, Colors::HARMONY_GOLD.b, (Uint8)(255 * scale)});
        gfx.draw_line(bp + Vec2(0, bh * scale), player->pos); // Connecting line

        if (scale > 0.8f) {
            for(int i=0; i < (int)d.replies.size(); ++i) {
                Color tc = (i == selected_reply) ? Colors::WHITE : Color(160, 160, 160, 255);
                gfx.draw_text(std::format("{} {}", (i == selected_reply ? ">" : " "), d.replies[i]), {bp.x + 10, bp.y + 15 + i * 35.0f}, 16, tc);
            }
            gfx.draw_text_centered("1-3 TO REPLY", {bp.x + bw/2, bp.y + bh + 15}, 12, Colors::HARMONY_GOLD);
            
            // Timer Bar
            float timer_pct = dialog_timer / 15.0f;
            gfx.draw_filled_rect(bp.x, bp.y + bh - 4, bw * timer_pct, 4, Colors::HARMONY_GOLD);
        }
    }

    void render_pause_menu() {
        gfx.draw_filled_rect(0, 0, WINDOW_W, WINDOW_H, {5, 5, 15, 180});
        gfx.draw_text_centered("VIGIL SUSPENDED", {static_cast<float>(WINDOW_W/2), static_cast<float>(WINDOW_H/2 - 50)}, 60, Colors::RESONANCE_GOLD);
        float pulse = 0.5f + 0.5f * std::sin(stability_timer * 4.0f);
        gfx.draw_text_centered("PRESS ESC TO RESUME PRAYER", {static_cast<float>(WINDOW_W/2), static_cast<float>(WINDOW_H/2 + 50)}, 24, {255, 255, 255, (Uint8)(255 * pulse)});
    }

    void render_evolution_overlay() {
        if (!evolution_pending) return;
        
        float entrance = std::min(1.0f, (10.0f - evolution_timer) * 4.0f);
        float exit = std::min(1.0f, evolution_timer * 2.0f);
        float scale = entrance * exit;
        if (scale <= 0) return;

        float card_w = 220, card_h = 140;
        float spacing = 20;
        float total_w = card_w * 3 + spacing * 2;
        float start_x = WINDOW_W / 2.0f - total_w / 2.0f;
        float base_y = WINDOW_H - 250.0f;

        const char* titles[] = {"RAY DENSITY", "HARMONY POWER", "RESONANCE RATE"};
        const char* desc[] = {"+1 Beam of Grace", "x1.45 Spiritual Power", "+1 Pierce, -10% CD"};

        for (int i = 0; i < 3; ++i) {
            float cx = start_x + i * (card_w + spacing);
            float cy = base_y + (1.0f - scale) * 100.0f; // Slide up animation

            gfx.draw_filled_rect(cx, cy, card_w, card_h, {10, 10, 30, (Uint8)(220 * scale)});
            gfx.draw_rect(cx, cy, card_w, card_h, {Colors::RESONANCE_GOLD.r, Colors::RESONANCE_GOLD.g, Colors::RESONANCE_GOLD.b, (Uint8)(255 * scale)});
            
            gfx.draw_text_centered(std::format("[{}]", i + 1), {cx + card_w/2, cy + 25}, 20, {Colors::HARMONY_GOLD.r, Colors::HARMONY_GOLD.g, Colors::HARMONY_GOLD.b, (Uint8)(255 * scale)});
            gfx.draw_text_centered(titles[i], {cx + card_w/2, cy + 60}, 18, {255, 255, 255, (Uint8)(255 * scale)});
            gfx.draw_text_centered(desc[i], {cx + card_w/2, cy + 100}, 14, {Colors::NEXUS_WHITE.r, Colors::NEXUS_WHITE.g, Colors::NEXUS_WHITE.b, (Uint8)(255 * scale)});
        }
        gfx.draw_text_centered("CHOOSE YOUR ASCENSION", {WINDOW_W/2.0f, base_y - 30.0f}, 18, {Colors::HARMONY_GOLD.r, Colors::HARMONY_GOLD.g, Colors::HARMONY_GOLD.b, (Uint8)(255 * scale)});
    }

    void render_messenger() {
        float bw = 1000, bh = 640; Vec2 center(WINDOW_W/2, WINDOW_H/2); float x = center.x - bw/2; float y = center.y - bh/2;
        gfx.draw_filled_rect(0, 0, WINDOW_W, WINDOW_H, {5, 5, 15, 200});
        gfx.draw_vignette(WINDOW_W, WINDOW_H, Colors::RADIANT_TEAL, 0.7f);
        gfx.draw_filled_rect(x, y, bw, bh, {10, 5, 20, 245});
        gfx.draw_rect(x, y, bw, bh, Colors::RESONANCE_GOLD);
        gfx.draw_rect(x + 5, y + 5, bw - 10, bh - 10, {200, 180, 50, 100});
        gfx.draw_text_centered("THE LENTEN ARCHIVE", {center.x, y + 60}, 42, Colors::RESONANCE_GOLD);
        
        gfx.draw_line({center.x - 200, y + 90}, {center.x + 200, y + 90});
        float sb_x = x + bw - 25; float sb_y = y + 110; float sb_h = bh - 180;
        gfx.draw_filled_rect(sb_x, sb_y, 10, sb_h, {50, 50, 80, 100});
        float total_h = static_cast<float>(divine_prose.size()) * 120.0f;
        float handle_h = std::max(30.0f, (sb_h / total_h) * sb_h);
        float handle_y = sb_y + (messenger_scroll / std::max(1.0f, total_h - sb_h)) * (sb_h - handle_h);
        gfx.draw_filled_rect(sb_x, handle_y, 10, handle_h, Colors::HARMONY_GOLD);
        SDL_Rect clip_rect = { static_cast<int>(x + 40), static_cast<int>(y + 110), static_cast<int>(bw - 100), static_cast<int>(bh - 180) };
        SDL_RenderSetClipRect(renderer, &clip_rect);
        for(int i = 0; i < (int)divine_prose.size(); ++i) {
            float line_y = y + 180 + i * 120.0f - messenger_scroll;
            if (line_y > y + 50 && line_y < y + bh - 50) {
                gfx.draw_text_centered_wrapped(divine_prose[i], {center.x + 2, line_y + 2}, 24, {0, 0, 0, 255}, (Uint32)(bw - 150));
                gfx.draw_text_centered_wrapped(divine_prose[i], {center.x, line_y}, 24, Colors::NEXUS_WHITE, (Uint32)(bw - 150));
            }
        }
        SDL_RenderSetClipRect(renderer, NULL); 
        float pulse = 0.7f + 0.3f * std::sin(stability_timer * 5.0f);
        gfx.draw_text_centered("REFLECT UPON THE WORD - CLICK TO RETURN", {center.x, y + bh - 40}, 18, {255, 230, 100, (Uint8)(255 * pulse)});
    }

public:
    Engine() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        audio = std::make_unique<AudioEngine>(); if (!audio->start()) std::cerr << "Warning: Audio system offline." << std::endl;
        TTF_Init(); window = SDL_CreateWindow("DIVINE NEXUS: LENTEN HARMONY - SACRED DESERT VIGIL", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        SDL_RenderSetLogicalSize(renderer, WINDOW_W, WINDOW_H); SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD); gfx.r = renderer;
        gfx.font = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 64); if (!gfx.font) gfx.font = TTF_OpenFont("/Library/Fonts/Arial.ttf", 64);
        for(int y = 0; y <= WINDOW_H; y += GRID_SIZE) { for(int x = 0; x <= WINDOW_W; x += GRID_SIZE) { grid.push_back({Vec2((float)x, (float)y), Vec2(0,0), Vec2(0,0)}); } }
        qtree = std::make_unique<QuadTree>(Rect{0, 0, (float)WINDOW_W, (float)WINDOW_H});
        for(int i=0; i<150; ++i) {
            Color star_c = (rand()%3==0) ? Colors::RADIANT_TEAL : ((rand()%2==0) ? Colors::RESONANCE_GOLD : Colors::NEXUS_WHITE);
            background_stars.push_back({ Vec2(rnd(0, WINDOW_W), rnd(0, WINDOW_H)), rnd(0.1f, 1.0f), rnd(0.5f, 2.0f), { star_c.r, star_c.g, star_c.b, (Uint8)rnd(40, 120) } });
        }
        for(int i=0; i<200; ++i) {
            desert_sands.push_back({ Vec2(rnd(0, WINDOW_W), rnd(0, WINDOW_H)), rnd(0.5f, 1.5f), rnd(0, 6.28f), rnd(5.0f, 15.0f) });
        }
        reset_game();
    }

    ~Engine() { audio.reset(); if (gfx.font) TTF_CloseFont(gfx.font); TTF_Quit(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); }

    void reset_game() {
        if (score > high_score) high_score = score;
        score = 0;
        entities.clear(); new_entities.clear(); pulses.clear(); text_particles.clear(); for(auto& p : particles) p.active = false;
        player = std::make_unique<Player>(Vec2(WINDOW_W/2, WINDOW_H/2)); glitch_spawn_timer = 0; stability_timer = 0; screenshake = 0; current_wave = 1; wave_timer = 0; wave_spawn_count = 0; boss_active = false;
        if (audio) { audio->set_binaural(432.0f, 40.0f, 0.15f); audio->set_player_state(0.5f, 0.0f, false, 0.0f); }
        trigger_divine_voice("COMMENCING HARMONIZATION PROTOCOL.");
    }

    void spawn_particle_chaos(Vec2 p, Color c, int count, float speed = 100.0f) {
        for(int i=0; i<count; ++i) {
            auto& d = particles[particle_ptr]; d.active = true; d.pos = p; float a = rnd(0, std::numbers::pi_v<float>*2);
            d.vel = Vec2(std::cos(a), std::sin(a)) * rnd(speed*0.5f, speed*1.5f); d.color = c; d.life = 1.0f; d.decay = rnd(1.0f, 3.0f); d.radius = rnd(1.5f, 3.5f);
            particle_ptr = (particle_ptr + 1) % MAX_PARTICLES;
        }
    }

    void add_bloom(Vec2 p, float r, Color c) { pulses.push_back({p, 0.0f, r, 1.0f, c}); }

    void apply_force_to_grid(Vec2 pos, float force, float radius) {
        for(auto& p : grid) { float d = p.base.dist(pos + p.offset); if(d < radius) { Vec2 dir = ( (p.base + p.offset) - pos).norm(); float f = (1.0f - d / radius) * force; p.vel = p.vel + dir * f; } }
    }

    void update_grid_parallel(float dt) {
        const float spring_k = 15.0f; const float damp = 0.92f; const size_t num_points = grid.size(); const size_t chunk_size = num_points / 4;
        float music_kick = 0.0f; if (audio) music_kick = audio->get_music_state().intensity;
        std::vector<std::future<void>> results;
        for (int i = 0; i < 4; ++i) {
            size_t start = i * chunk_size; size_t end = (i == 3) ? num_points : (i + 1) * chunk_size;
            results.push_back(workers.enqueue([this, start, end, dt, spring_k, damp, music_kick] {
                for (size_t j = start; j < end; ++j) {
                    auto& p = grid[j]; Vec2 spring_force = p.offset * -spring_k;
                    if (music_kick > 0.5f) { float d = p.base.dist(player->pos); spring_force = spring_force + (p.base - player->pos).norm() * (music_kick * 20.0f / (1.0f + d * 0.01f)); }
                    p.vel = p.vel + spring_force * dt; p.offset = p.offset + p.vel * dt; p.vel = p.vel * damp;
                }
            }));
        }
        for (auto& f : results) f.wait();
    }
    
    // Optimization: Parallel Particle Updates
    void update_particles_parallel(float dt) {
        const size_t chunk_size = MAX_PARTICLES / 4;
        std::vector<std::future<void>> results;
        for (int i = 0; i < 4; ++i) {
            size_t start = i * chunk_size; size_t end = (i == 3) ? MAX_PARTICLES : (i + 1) * chunk_size;
            results.push_back(workers.enqueue([this, start, end, dt] {
                for (size_t j = start; j < end; ++j) {
                    auto& p = particles[j];
                    if(!p.active) continue;
                    p.pos = p.pos + p.vel * dt;
                    p.life -= p.decay * dt;
                    p.vel = p.vel * 0.96f;
                    if(p.life <= 0) p.active = false;
                }
            }));
        }
        for (auto& f : results) f.wait();
    }

    std::generator<Vec2> spawn_wave_generator(int count) { for (int i = 0; i < count; ++i) co_yield generate_spawn_pos(); }

    void update(float dt) {
        float glitch_speed_mult = 1.0f; float music_intensity = 0.0f;
        if (audio) {
            float x_norm = player->pos.x / (float)WINDOW_W; float speed_norm = player->vel.mag() / player->speed;
            float progress = player->harmony_xp / player->harmony_next; audio->set_player_state(x_norm, speed_norm, state == State::PLAYING, progress);
            auto m_state = audio->get_music_state(); bg_flash = std::lerp(bg_flash, m_state.intensity * 0.15f, 0.1f);
            player->internal_pulse = 1.0f + m_state.intensity * 0.25f; glitch_speed_mult = 1.0f + m_state.intensity * 0.45f; music_intensity = m_state.intensity;
        }

        if (state != State::PLAYING || messenger_open) return;
        stability_timer += dt;
        
        // Random Dialogue Trigger (More frequent for visibility)
        if (rand() % 2000 == 0) trigger_dialogue();
        
        if (current_dialog_idx != -1) {
            dialog_timer -= dt;
            if (dialog_timer <= 0) current_dialog_idx = -1;
        }

        if (evolution_pending) {
            evolution_timer -= dt;
            if (evolution_timer <= 0) evolution_pending = false;
        }
        if (screenshake > 0) screenshake -= dt * 30.0f; else screenshake = 0;
        bg_flash = std::max(0.0f, bg_flash - dt * 0.5f);
        if (combo_timer > 0) combo_timer -= dt; else combo_count = 0;

        scripture_timer += dt;
        if (scripture_timer > 10.0f) { scripture_timer = 0; current_scripture_idx = (current_scripture_idx + 1) % scriptures.size(); }

        divine_msg_timer -= dt;
        if (divine_msg_timer < -15.0f) trigger_divine_voice(idle_lenten_quotes[rand() % idle_lenten_quotes.size()]);

        fruit_spawn_timer -= dt;
        if (fruit_spawn_timer <= 0) {
            fruit_spawn_timer = rnd(20.0f, 40.0f);
            static const std::pair<std::string, Color> fruits[] = { 
                {"LOVE", {255, 50, 50, 255}}, 
                {"JOY", Colors::HARMONY_GOLD}, 
                {"PEACE", Colors::RADIANT_TEAL}, 
                {"PATIENCE", Colors::WHITE},
                {"KINDNESS", {255, 150, 200, 255}},
                {"GOODNESS", {100, 255, 100, 255}},
                {"FAITHFULNESS", {100, 100, 255, 255}},
                {"GENTLENESS", {200, 200, 255, 255}},
                {"SELF-CONTROL", {255, 100, 50, 255}}
            };
            int idx = rand() % 9; 
            new_entities.push_back(std::make_unique<FruitOfSpirit>(Vec2(rnd(100, WINDOW_W-100), -50), fruits[idx].first, fruits[idx].second));
        }

        if (player->blessing_timer > 0) player->blessing_timer -= dt; else player->active_virtue = "";
        float power_mult = (player->active_virtue == "LOVE") ? 2.0f : 1.0f;
        float speed_mult = (player->active_virtue == "JOY") ? 1.5f : 1.0f;
        player->siphon_range = (player->active_virtue == "PEACE") ? 500.0f : 250.0f;
        float enemy_slow = (player->active_virtue == "PATIENCE") ? 0.4f : 1.0f;

        gfx.cam_offset = Vec2(rnd(-screenshake, screenshake), rnd(-screenshake, screenshake));
        const Uint8* keys = SDL_GetKeyboardState(NULL); Vec2 move_input;
        if (keys[SDL_SCANCODE_W]) { move_input.y -= 1; }
        if (keys[SDL_SCANCODE_S]) { move_input.y += 1; }
        if (keys[SDL_SCANCODE_A]) { move_input.x -= 1; }
        if (keys[SDL_SCANCODE_D]) { move_input.x += 1; }
        
        if (move_input.mag() > 0) { 
            Vec2 move_dir = move_input.norm(); 
            player->vel = player->vel + move_dir * player->speed * speed_mult * 8.0f * dt; 
            apply_force_to_grid(player->pos, 12.0f, 120.0f); 
        }
        int mx, my; SDL_GetMouseState(&mx, &my); player->angle = std::atan2(static_cast<float>(my) - player->pos.y, static_cast<float>(mx) - player->pos.x);

        if (keys[SDL_SCANCODE_SPACE] && player->dash_cooldown <= 0) { player->vel = move_input.norm() * 1500.0f; player->dash_cooldown = 1.0f; screenshake = 15.0f; spawn_particle_chaos(player->pos, Colors::RADIANT_TEAL, 30); apply_force_to_grid(player->pos, 180.0f, 300.0f); add_bloom(player->pos, 250.0f, Colors::RADIANT_TEAL); if (audio) audio->play_warp_jump(); }
        
        // PRAYER MECHANIC: Key P
        static float prayer_cooldown = 0.0f;
        if (prayer_cooldown > 0) prayer_cooldown -= dt;
        if (keys[SDL_SCANCODE_P] && prayer_cooldown <= 0) {
            prayer_cooldown = 5.0f;
            player->integrity = std::min(player->max_integrity, player->integrity + 10.0f);
            screenshake = 0; // Moment of peace
            add_bloom(player->pos, 500.0f, Colors::RESONANCE_GOLD);
            spawn_text_particle(player->pos, "SHORT PRAYER", Colors::RESONANCE_GOLD);
            if (audio) audio->play_restoration_sigh();
            apply_force_to_grid(player->pos, -100.0f, 400.0f); // Gentle pull towards player
        }

        if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT) && player->harmony_timer <= 0) {
            player->harmony_timer = player->harmony_rate; Vec2 fwd(std::cos(player->angle), std::sin(player->angle)); player->vel = player->vel - fwd * 70.0f; screenshake = 2.5f; if (audio) audio->play_divine_ray();
            for (int i = 0; i < player->ray_count; ++i) { float angle_offset = (i - (player->ray_count-1)/2.0f) * player->ray_spread; Vec2 dir(std::cos(player->angle + angle_offset), std::sin(player->angle + angle_offset)); new_entities.push_back(std::make_unique<HarmonyRay>(player->pos + dir * 20, dir * player->ray_speed, player->harmony_power * power_mult, Colors::NEXUS_WHITE, player->ray_pierce)); }
        }

        player->update(dt); update_grid_parallel(dt); update_particles_parallel(dt);
        
        if (audio) {
            // Dynamic Binaural Modulation
            // Carrier shifts up with level, beat frequency increases as integrity drops (tension)
            float base_carrier = 432.0f + (player->level - 1) * 12.0f;
            float target_beat = 40.0f * (1.0f + (1.0f - player->integrity/player->max_integrity) * 0.5f);
            audio->set_binaural(base_carrier, target_beat, 0.15f + music_intensity * 0.1f);
        }

        for(auto& s : background_stars) { 
            s.pos = s.pos - player->vel * (dt * s.depth * 0.15f); 
            if(s.pos.x < 0) { s.pos.x += WINDOW_W; }
            if(s.pos.x > WINDOW_W) { s.pos.x -= WINDOW_W; }
            if(s.pos.y < 0) { s.pos.y += WINDOW_H; }
            if(s.pos.y > WINDOW_H) { s.pos.y -= WINDOW_H; }
            // CRUCIFORM CONSTELLATION PULSE
            if(rand() % 5000 == 0) s.size = 5.0f; // Flare
        }
        for(auto& sand : desert_sands) {
            sand.pos.x -= sand.speed * dt;
            sand.pos.y += std::sin(sand.phase + stability_timer) * 5.0f * dt;
            sand.phase += dt;
            if(sand.pos.x < 0) sand.pos.x += WINDOW_W;
        }
        for(auto& p : pulses) { p.radius += 600.0f * dt; p.life -= dt * 1.8f; } std::erase_if(pulses, [](const auto& p) { return p.life <= 0; });
        for(auto& tp : text_particles) { tp.pos.y -= 100.0f * dt; tp.life -= dt * 1.5f; } std::erase_if(text_particles, [](const auto& tp) { return tp.life <= 0; });

        glitch_spawn_timer -= dt; wave_timer += dt;
        int max_wave_spawns = 5 + current_wave * 3; float spawn_interval = 2.0f / (1.0f + current_wave * 0.2f);
        if (glitch_spawn_timer <= 0 && wave_spawn_count < max_wave_spawns) {
            float difficulty = current_wave + (stability_timer / 60.0f); glitch_spawn_timer = spawn_interval;
            for (auto spawn_pos : spawn_wave_generator(1)) {
                ObjType t = ObjType::GLITCH_BASIC; if (current_wave >= 2 && rand()%4==0) t = ObjType::GLITCH_DASH; if (current_wave >= 3 && rand()%5==0) t = ObjType::GLITCH_SPLITTER; if (current_wave >= 4 && rand()%5==0) t = ObjType::GLITCH_TANK;
                auto glitch_res = create_glitch(spawn_pos, t, difficulty); if (glitch_res) { new_entities.push_back(std::move(*glitch_res)); apply_force_to_grid(spawn_pos, 60.0f, 180.0f); wave_spawn_count++; }
            }
        }

        bool entities_clear = std::none_of(entities.begin(), entities.end(), [](const auto& e) { return e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_SPLITTER; });
        if (wave_spawn_count >= max_wave_spawns && entities_clear && !boss_active) {
            Vec2 spawn_pos = generate_spawn_pos(); auto boss_res = create_glitch(spawn_pos, ObjType::GLITCH_BOSS, current_wave * 0.5f);
            if (boss_res) { new_entities.push_back(std::move(*boss_res)); add_bloom(spawn_pos, 600.0f, Colors::RED); apply_force_to_grid(spawn_pos, 400.0f, 700.0f); if (audio) audio->play_glitch_spawn(); screenshake = 30.0f; boss_active = true; trigger_divine_voice("BE SOBER, BE VIGILANT. THE ADVERSARY APPROACHES."); }
        }
        bool boss_exists = std::any_of(entities.begin(), entities.end(), [](const auto& e) { return e->type == ObjType::GLITCH_BOSS; });
        if (boss_active && !boss_exists) { current_wave++; wave_spawn_count = 0; boss_active = false; screenshake = 45.0f; if (audio) audio->play_restoration_sigh(); trigger_divine_voice("THE LIGHT SHINES IN THE DARKNESS."); }
        if (player->level >= 10) state = State::VICTORY;

        qtree->clear(); for (auto& e : entities) { if (e->type != ObjType::PARTICLE) qtree->insert(e.get()); }
        for (auto& e : entities) {
            e->update(dt); if (e->ascended) continue;
            if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_BOSS || e->type == ObjType::GLITCH_SPLITTER) {
                Glitch* g = static_cast<Glitch*>(e.get()); Vec2 dir = (player->pos - e->pos).norm();
                if (e->type == ObjType::GLITCH_DASH) {
                    if (g->internal_state == 0) { e->vel = e->vel + dir * 150.0f * dt * glitch_speed_mult * enemy_slow; if (g->state_timer <= 0) { g->internal_state = 1; g->state_timer = 0.8f; } }
                    else if (g->internal_state == 1) { e->vel = e->vel * 0.9f; if (g->state_timer <= 0) { g->internal_state = 2; g->state_timer = 0.4f; e->vel = dir * 1350.0f * glitch_speed_mult * enemy_slow; apply_force_to_grid(e->pos, 100.0f, 200.0f); spawn_particle_chaos(e->pos, g->color, 15, 100.0f); } }
                    else if (g->internal_state == 2) { if (g->state_timer <= 0) { g->internal_state = 0; g->state_timer = 2.0f; } }
                } else if (e->type == ObjType::GLITCH_BOSS) {
                    e->vel = e->vel + dir * 45.0f * dt * 2.2f * glitch_speed_mult * enemy_slow;
                    if (g->state_timer <= 0) { 
                        int pattern = rand() % 3;
                        if (pattern == 0) { // Nova
                            for(int i=0; i<12; ++i) { 
                                float a = i * (std::numbers::pi_v<float> * 2.0f / 12.0f); 
                                Vec2 pdir(std::cos(a), std::sin(a)); 
                                new_entities.push_back(std::make_unique<GlitchProjectile>(e->pos + pdir * 70.0f, pdir * 320.0f * enemy_slow)); 
                            }
                            g->state_timer = 2.5f;
                        } else if (pattern == 1) { // Spiral
                            for(int i=0; i<16; ++i) {
                                float a = i * 0.4f;
                                Vec2 pdir(std::cos(a), std::sin(a));
                                new_entities.push_back(std::make_unique<GlitchProjectile>(e->pos, pdir * (200.0f + i * 10.0f) * enemy_slow));
                            }
                            g->state_timer = 3.5f;
                        } else { // Focused Burst
                            for(int i=0; i<5; ++i) {
                                float a = player->angle + rnd(-0.3f, 0.3f) + std::numbers::pi_v<float>;
                                Vec2 pdir(std::cos(a), std::sin(a));
                                new_entities.push_back(std::make_unique<GlitchProjectile>(e->pos, pdir * 450.0f * enemy_slow));
                            }
                            g->state_timer = 1.5f;
                        }
                    }
                } else if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_SPLITTER) {
                    if (g->state_timer <= 0) { float roll = rnd(0, 1); if (g->internal_state == 0) g->internal_state = (roll < 0.7f) ? 1 : 2; else if (g->internal_state == 1) g->internal_state = (roll < 0.5f) ? 0 : 2; else g->internal_state = (roll < 0.6f) ? 0 : 1; g->state_timer = rnd(0.8f, 2.5f); }
                    if (g->internal_state == 0) e->vel = e->vel + dir * 250.0f * dt * glitch_speed_mult * enemy_slow; else if (g->internal_state == 1) e->vel = e->vel + dir * 600.0f * dt * glitch_speed_mult * enemy_slow; else { Vec2 side(-dir.y, dir.x); e->vel = e->vel + (dir * 0.8f + side) * 350.0f * dt * glitch_speed_mult * enemy_slow; }
                } else if (e->type == ObjType::GLITCH_TANK) {
                    e->vel = e->vel + dir * 55.0f * dt * 2.2f * glitch_speed_mult * enemy_slow;
                    std::vector<GameObject*> nearby; qtree->query(Rect{e->pos.x - 150, e->pos.y - 150, 300, 300}, nearby);
                    for (auto n : nearby) { if (n != e.get() && (n->type == ObjType::GLITCH_BASIC || n->type == ObjType::GLITCH_DASH || n->type == ObjType::GLITCH_SPLITTER)) { static_cast<Glitch*>(n)->stability = std::min(static_cast<Glitch*>(n)->max_stability, static_cast<Glitch*>(n)->stability + 8.0f * dt); } }
                }
                e->vel = e->vel * 0.95f;
                if (e->pos.dist(player->pos) < e->radius + player->radius) { player->integrity -= (e->type == ObjType::GLITCH_BOSS) ? 35 : 20; screenshake = (e->type == ObjType::GLITCH_BOSS) ? 35.0f : 15.0f; hit_stop = 0.1f; spawn_particle_chaos(player->pos, Colors::GOLD, 40); apply_force_to_grid(player->pos, 100.0f, 250.0f); if (audio) audio->play_player_damage(); trigger_divine_voice("I AM WITH YOU ALWAYS."); e->ascended = (e->type != ObjType::GLITCH_BOSS); if (player->integrity <= 0) state = State::GAME_OVER; }
            }
            if (e->type == ObjType::GLITCH_PROJECTILE) { 
                if (e->pos.dist(player->pos) < e->radius + player->radius) { player->integrity -= 5; screenshake = 8.0f; spawn_particle_chaos(player->pos, Colors::RED, 15); if (audio) audio->play_player_damage(); e->ascended = true; if (player->integrity <= 0) state = State::GAME_OVER; } 
            }
            if (e->type == ObjType::HARMONY_PULSE) {
                HarmonyPulse* p = static_cast<HarmonyPulse*>(e.get());
                apply_force_to_grid(p->pos, 50.0f, p->current_r);
                
                std::vector<GameObject*> nearby;
                qtree->query(Rect{p->pos.x - p->current_r, p->pos.y - p->current_r, p->current_r * 2, p->current_r * 2}, nearby);
                for (auto n : nearby) {
                    if (n->pos.dist(p->pos) < p->current_r) {
                        if (n->type == ObjType::GLITCH_PROJECTILE) n->ascended = true;
                        if (n->type == ObjType::GLITCH_BASIC || n->type == ObjType::GLITCH_DASH || n->type == ObjType::GLITCH_TANK || n->type == ObjType::GLITCH_SPLITTER) {
                            static_cast<Glitch*>(n)->stability -= 50.0f * dt; // Continuous stabilization
                            static_cast<Glitch*>(n)->hit_timer = 0.1f;
                        }
                    }
                }
            }
            if (e->type == ObjType::HARMONY_RAY) {
                HarmonyRay* ray = static_cast<HarmonyRay*>(e.get());
                
                // CCD: Define the swept area for the quadtree query
                float min_x = std::min(ray->pos.x, ray->prev_pos.x) - 30.0f;
                float max_x = std::max(ray->pos.x, ray->prev_pos.x) + 30.0f;
                float min_y = std::min(ray->pos.y, ray->prev_pos.y) - 30.0f;
                float max_y = std::max(ray->pos.y, ray->prev_pos.y) + 30.0f;
                
                std::vector<GameObject*> targets;
                qtree->query(Rect{min_x, min_y, max_x - min_x, max_y - min_y}, targets);
                
                for (auto target : targets) {
                    if (target->type == ObjType::GLITCH_BASIC || target->type == ObjType::GLITCH_TANK || target->type == ObjType::GLITCH_DASH || target->type == ObjType::GLITCH_BOSS || target->type == ObjType::GLITCH_SPLITTER) {
                        uintptr_t tid = reinterpret_cast<uintptr_t>(target);
                        if (ray->hits.find(tid) == ray->hits.end()) {
                            // CCD: Check distance from target to the LINE SEGMENT of the bullet path
                            float d = Vec2::dist_to_segment(target->pos, ray->prev_pos, ray->pos);
                            if (d < target->radius + ray->radius) {
                                Glitch* g = static_cast<Glitch*>(target);
                                g->stability -= ray->damage;
                                g->hit_timer = 0.15f; 
                                ray->hits.insert(tid);
                                spawn_particle_chaos(ray->pos, Colors::NEXUS_WHITE, 5); 
                                if (rand()%2==0) spawn_particle_chaos(ray->pos, Colors::HARMONY_GOLD, 2, 30.0f);
                                apply_force_to_grid(ray->pos, 20.0f, 100.0f); if (audio) audio->play_impact(); 
                                if (ray->pierce_left <= 0) ray->ascended = true; else {
                                    ray->pierce_left--;
                                    ray->damage *= 1.2f; // Resonance: increased power per pierce
                                    player->integrity = std::min(player->max_integrity, player->integrity + 1.0f);
                                    spawn_text_particle(ray->pos, "RESONANCE", Colors::HARMONY_GOLD);
                                }
                                if (g->stability <= 0) {
                                    g->ascended = true; combo_count++; combo_timer = 1.5f; 
                                    score += (g->type == ObjType::GLITCH_BOSS) ? 1000 : 100;
                                    score += combo_count * 10;
                                    
                                    // Trigger Harmony Pulse on combo milestones
                                    if (combo_count % 10 == 0) {
                                        new_entities.push_back(std::make_unique<HarmonyPulse>(g->pos, 400.0f, Colors::RADIANT_TEAL));
                                        if (audio) audio->play_restoration_sigh();
                                        trigger_divine_voice("MASS ASCENSION DETECTED.");
                                    }

                                    spawn_particle_chaos(g->pos, Colors::GOLD, (g->type == ObjType::GLITCH_BOSS) ? 200 : 40, 180.0f);
                                    spawn_particle_chaos(g->pos, Colors::WHITE, (g->type == ObjType::GLITCH_BOSS) ? 100 : 20, 280.0f);
                                    static const std::string restorative_notes[] = {"ASCENDED", "RESTORED", "INTO BEING", "TO LIGHT", "AMEN"}; 
                                    for(int i=0; i<3; ++i) { spawn_text_particle(g->pos + Vec2(rnd(-40, 40), rnd(-40, 40)), restorative_notes[rand()%5], Colors::NEXUS_WHITE); }
                                    
                                    float h_force = (g->type == ObjType::GLITCH_BOSS) ? 600.0f : 250.0f;
                                    apply_force_to_grid(g->pos, h_force * (1.0f + music_intensity), 600.0f); 
                                    add_bloom(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 650.0f : 220.0f, g->color);
                                    if (audio) { audio->play_harmonic_combo(combo_count); if (g->type == ObjType::GLITCH_BOSS) audio->play_restoration_sigh(); }
                                    if (g->type == ObjType::GLITCH_SPLITTER) { for(int i=0; i<4; ++i) { auto mini = create_glitch(g->pos + Vec2(rnd(-20, 20), rnd(-20, 20)), ObjType::GLITCH_BASIC, current_wave * 0.5f); if (mini) { (*mini)->radius = 7.0f; (*mini)->color = Colors::RADIANT_TEAL; new_entities.push_back(std::move(*mini)); } } }
                                    new_entities.push_back(std::make_unique<HarmonyOrb>(g->pos, (g->type == ObjType::GLITCH_BOSS) ? 150.0f : 15.0f));
                                    bg_flash = std::min(0.5f, bg_flash + (g->type == ObjType::GLITCH_BOSS ? 0.4f : 0.08f));
                                }
                                break;
                            }
                        }
                    }
                }
                if (!ray->ascended && ray->life < dt) { if (audio) audio->play_dissipate(); }
            }
            if (e->type == ObjType::FRUIT_OF_SPIRIT) {
                if (e->pos.dist(player->pos) < e->radius + player->radius) {
                    FruitOfSpirit* f = static_cast<FruitOfSpirit*>(e.get()); player->active_virtue = f->virtue; player->blessing_timer = 8.0f; 
                    spawn_particle_chaos(player->pos, f->color, 30); 
                    add_bloom(player->pos, 300.0f, f->color); 
                    
                    // Powerful Resonance Pulse
                    new_entities.push_back(std::make_unique<HarmonyPulse>(player->pos, 600.0f, f->color));
                    apply_force_to_grid(player->pos, 400.0f, 500.0f);

                    if (audio) audio->play_collect_harmony(); 
                    trigger_divine_voice("GRACE IS POURED ABROAD IN THY LIPS."); 
                    e->ascended = true;
                }
            }
        }

        std::vector<GameObject*> nearby_orbs; qtree->query(Rect{player->pos.x - 300, player->pos.y - 300, 600, 600}, nearby_orbs);
        int siphoning_count = 0;
        for (auto orb_ptr : nearby_orbs) {
            if (orb_ptr->type == ObjType::HARMONY_ORB) {
                float dist = orb_ptr->pos.dist(player->pos);
                if (dist < player->siphon_range) { float pull_force = 1500.0f * (1.0f - dist / player->siphon_range); orb_ptr->vel = orb_ptr->vel + (player->pos - orb_ptr->pos).norm() * pull_force * dt; siphoning_count++; gfx.set_color(Colors::HARMONY_GOLD, 0.3f); gfx.draw_line(orb_ptr->pos, player->pos); }
                if (dist < player->radius + 15.0f) { 
                    orb_ptr->ascended = true; 
                    player->harmony_xp += static_cast<HarmonyOrb*>(orb_ptr)->value; 
                    if (audio) audio->play_collect_harmony(); 
                    if (player->harmony_xp >= player->harmony_next) { 
                        player->harmony_xp = 0; 
                        player->harmony_next *= 1.25f; 
                        player->level++; 
                        evolution_pending = true;
                        evolution_timer = 10.0f; // Options stay for 10 seconds or until chosen
                        if (audio) audio->play_divine_ascent(); 
                        trigger_divine_voice("ASCEND IN VIRTUE."); 
                    } 
                }
            }
        }
        if (siphoning_count > 0) player->integrity = std::min(player->max_integrity, player->integrity + siphoning_count * 0.5f * dt);
        for(auto& ne : new_entities) entities.push_back(std::move(ne));
        new_entities.clear(); std::erase_if(entities, [](const auto& e) { return e->ascended; });
    }

    void render() {
        Color bg = Colors::BG_DARK; SDL_SetRenderDrawColor(renderer, (Uint8)(bg.r + bg_flash * 100), (Uint8)(bg.g + bg_flash * 150), (Uint8)(bg.b + bg_flash * 255), 255); SDL_RenderClear(renderer);
        float music_kick = (audio) ? audio->get_music_state().intensity : 0.0f;
        
        // LITURGICAL VIGNETTE
        gfx.draw_vignette(WINDOW_W, WINDOW_H, {80, 0, 120, 255}, 0.2f + music_kick * 0.3f);

        for(const auto& sand : desert_sands) {
            float shimmer = 0.3f + 0.7f * std::abs(std::sin(sand.phase * 2.0f));
            gfx.set_color(Colors::RESONANCE_GOLD, shimmer * 0.4f);
            gfx.draw_circle(sand.pos, sand.size);
        }

        for(const auto& s : background_stars) { float pulse = 1.0f + bg_flash * 3.0f; gfx.set_color(s.color, std::min(1.0f, (s.color.a / 255.0f) * pulse)); gfx.draw_circle(s.pos, s.size * pulse); }
        int cols = (WINDOW_W / GRID_SIZE) + 1; int movement = (audio) ? audio->get_music_state().movement : 0;
        Color base_grid = {70, 140, 255, 30}; if (movement == 1) base_grid = {255, 100, 200, 30}; else if (movement == 2) base_grid = {255, 230, 100, 30}; else if (movement == 3) base_grid = {100, 255, 200, 30}; else if (movement == 4) base_grid = {200, 100, 255, 30}; 
        for(size_t i = 0; i < grid.size() ; ++i) { size_t x = i % cols; size_t y = i / cols; Vec2 p1 = grid[i].base + grid[i].offset; float dist_sq = grid[i].offset.x * grid[i].offset.x + grid[i].offset.y * grid[i].offset.y; float intensity = std::min(1.0f, dist_sq / 400.0f); float d_player = p1.dist(player->pos); float light = std::max(0.0f, 1.0f - d_player / 150.0f);
            // GOLDEN SHIMMER GRID
            float shimmer = 0.8f + 0.2f * std::sin(stability_timer * 20.0f + static_cast<float>(i));
            Color grid_c = lerp_color(base_grid, {255, 255, 255, 220}, std::max(intensity, light) * shimmer); gfx.set_color(grid_c);
            if (x < (size_t)cols - 1) {
                gfx.draw_line(p1, grid[i+1].base + grid[i+1].offset);
            }
            if (y < (size_t)(grid.size()/cols) - 1) {
                gfx.draw_line(p1, grid[i+cols].base + grid[i+cols].offset);
            }
        }
        for(const auto& p : pulses) gfx.draw_bloom(p.pos, p.radius, p.color);
        for(const auto& tp : text_particles) { gfx.draw_text(tp.text, tp.pos, 24, {tp.color.r, tp.color.g, tp.color.b, (Uint8)(255 * tp.life)}); }
        for(const auto& p : particles) { if(!p.active) continue; gfx.set_color(p.color, p.life); float s = p.radius * p.life; gfx.draw_line(p.pos - Vec2(s, 0), p.pos + Vec2(s, 0)); gfx.draw_line(p.pos - Vec2(0, s), p.pos + Vec2(0, s)); }
        
        if (state == State::PLAYING || state == State::EVOLUTION || state == State::GAME_OVER || state == State::VICTORY) {
            float grace_pct = player->integrity / player->max_integrity; Color grace_c = lerp_color(Colors::RED, Colors::HARMONY_GOLD, grace_pct);
            gfx.draw_glowing_circle({60, 60}, 30.0f * (0.8f + 0.2f * std::sin(stability_timer * 10.0f)), grace_c);
            gfx.draw_text(std::format("{:.0f}%", player->integrity), {45, 50}, 20, Colors::WHITE); gfx.draw_text("GRACE", {40, 100}, 18, Colors::HARMONY_GOLD);
            
            gfx.draw_text(std::format("SCORE {:06d}", score), {40, 140}, 22, Colors::WHITE);
            gfx.draw_text(std::format("BEST {:06d}", high_score), {40, 170}, 16, Colors::HARMONY_GOLD);
            
            // RADIANT NEXUS CROSS (Top Right)
            int mx, my; SDL_GetMouseState(&mx, &my);
            bool hover = (mx > WINDOW_W - 120 && my < 120);
            float cross_flare = hover ? 1.5f : 1.0f;
            gfx.draw_cross({static_cast<float>(WINDOW_W - 60), 60}, 20 * cross_flare, Colors::HARMONY_GOLD, 3.0f, true);
            gfx.draw_bloom({static_cast<float>(WINDOW_W - 60), 60}, 35 * (1.0f + music_kick), Colors::HARMONY_GOLD);
            gfx.draw_text("WISDOM", {static_cast<float>(WINDOW_W - 90), 100}, 18, Colors::HARMONY_GOLD);

            if (divine_msg_timer > 0) {
                float alpha = std::min(1.0f, divine_msg_timer); Vec2 b_pos(WINDOW_W - 350, 150); float bw = 300, bh = 80;
                gfx.draw_filled_rect(b_pos.x, b_pos.y, bw, bh, {10, 15, 35, (Uint8)(230 * alpha)});
                gfx.draw_rect(b_pos.x, b_pos.y, bw, bh, {Colors::HARMONY_GOLD.r, Colors::HARMONY_GOLD.g, Colors::HARMONY_GOLD.b, (Uint8)(255 * alpha)});
                gfx.draw_line({static_cast<float>(WINDOW_W - 60), 100}, {b_pos.x + bw/2, b_pos.y}); 
                gfx.draw_text_centered_wrapped(current_divine_msg, {b_pos.x + bw/2, b_pos.y + bh/2}, 18, {255, 255, 255, (Uint8)(255 * alpha)}, (Uint32)bw - 20);
            }

            float xp_pct = player->harmony_xp / player->harmony_next;
            gfx.draw_filled_rect(WINDOW_W/2 - 200, WINDOW_H - 80, 400, 6, {50, 50, 50, 150}); gfx.draw_filled_rect(WINDOW_W/2 - 200, WINDOW_H - 80, 400 * xp_pct, 6, Colors::HARMONY_GOLD);
            gfx.draw_text_centered(std::format("FAITH LEVEL {}", player->level), {static_cast<float>(WINDOW_W/2), static_cast<float>(WINDOW_H - 100)}, 18, Colors::WHITE);

            for (auto& e : entities) {
                // Off-screen indicators
                if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_BOSS || e->type == ObjType::GLITCH_SPLITTER) {
                    if (e->pos.x < 0 || e->pos.x > WINDOW_W || e->pos.y < 0 || e->pos.y > WINDOW_H) {
                        Vec2 center(WINDOW_W/2, WINDOW_H/2);
                        Vec2 dir = (e->pos - center).norm();
                        float angle = std::atan2(dir.y, dir.x);
                        Vec2 edge_pos = center + dir * 350.0f; // Positioned near the edge
                        edge_pos.x = std::clamp(edge_pos.x, 20.0f, (float)WINDOW_W - 20.0f);
                        edge_pos.y = std::clamp(edge_pos.y, 20.0f, (float)WINDOW_H - 20.0f);
                        gfx.draw_indicator(edge_pos, angle, e->color);
                    }
                }

                if (e->type == ObjType::HARMONY_RAY) {
                    Vec2 fwd = e->vel.norm();
                    float bolt_len = 8.0f; // Shorter, more concentrated bolt
                    // Draw sharp bolt of light
                    for(int i=0; i<6; ++i) {
                        float t = i / 6.0f;
                        float width = (1.0f - t) * 3.5f;
                        Vec2 p1 = e->pos - fwd * (i * bolt_len);
                        Vec2 p2 = e->pos - fwd * ((i+1) * bolt_len);
                        
                        gfx.set_color(e->color, (1.0f - t) * 0.8f);
                        for(float offset = -width; offset <= width; offset += 1.0f) {
                            Vec2 side(-fwd.y, fwd.x);
                            gfx.draw_line(p1 + side * offset, p2 + side * offset);
                        }
                        gfx.set_color(Colors::WHITE, (1.0f - t));
                        gfx.draw_line(p1, p2);
                    }
                    if (rand()%5==0) spawn_particle_chaos(e->pos, Colors::NEXUS_WHITE, 1, 20.0f);
                    gfx.draw_bloom(e->pos, e->radius * 1.5f, e->color);
                } else if (e->type == ObjType::HARMONY_PULSE) {
                    HarmonyPulse* p = static_cast<HarmonyPulse*>(e.get());
                    float alpha = 1.0f - (p->current_r / p->max_r);
                    gfx.set_color(p->color, alpha * 0.5f);
                    gfx.draw_circle(p->pos, p->current_r);
                    gfx.set_color(Colors::WHITE, alpha * 0.8f);
                    gfx.draw_circle(p->pos, p->current_r - 2.0f);
                    gfx.draw_bloom(p->pos, p->current_r, p->color);
                } else if (e->type == ObjType::HARMONY_ORB) {
                    HarmonyOrb* orb = static_cast<HarmonyOrb*>(e.get());
                    float p = std::abs(std::sin(orb->pulse_timer));
                    float size = orb->radius * (0.8f + p * 0.4f);
                    gfx.draw_glowing_circle(e->pos, size, e->color);
                    // Sacred cross inside the orb to distinguish from projectiles
                    gfx.draw_cross(e->pos, size * 0.6f, Colors::WHITE, 1.0f);
                } else if (e->type != ObjType::PLAYER) {
                    if (e->type == ObjType::GLITCH_BASIC || e->type == ObjType::GLITCH_DASH || e->type == ObjType::GLITCH_TANK || e->type == ObjType::GLITCH_BOSS || e->type == ObjType::GLITCH_SPLITTER) {
                        Glitch* g = static_cast<Glitch*>(e.get()); float hit_scale = 1.0f + (g->hit_timer > 0 ? g->hit_timer * 2.0f : 0.0f); Color draw_c = g->hit_timer > 0 ? Colors::WHITE : g->color;
                        if (e->type == ObjType::GLITCH_DASH) { 
                        if (g->internal_state == 1) { 
                            if ((int)(g->state_timer * 24) % 2 == 0) draw_c = Colors::WHITE; 
                        } else if (g->internal_state == 2) { 
                            for(int i=1; i<6; ++i) {
                                Vec2 jitter(rnd(-2, 2), rnd(-2, 2));
                                gfx.draw_wireframe_3d(g->pos - e->vel * (i * 0.012f) + jitter, g->vertices, g->edges, g->rx, g->ry, g->rz, g->radius * (1.0f - i*0.15f), {draw_c.r, draw_c.g, draw_c.b, (Uint8)(150 / i)}); 
                            }
                        } 
                    }
                        gfx.draw_wireframe_3d(g->pos, g->vertices, g->edges, g->rx, g->ry, g->rz, g->radius * hit_scale, draw_c);
                        // Inner Divine Spark (for visibility and theme)
                        gfx.draw_glowing_circle(g->pos, 2.0f, Colors::NEXUS_WHITE);
                        
                        if (e->type == ObjType::GLITCH_TANK) { 
                            float aura_pulse = 1.0f + 0.1f * std::sin(stability_timer * 4.0f);
                            gfx.draw_wireframe_3d(g->pos, g->vertices, g->edges, -g->rx, -g->ry, -g->rz, g->aura_radius * aura_pulse, {g->color.r, g->color.g, g->color.b, 60}); 
                            gfx.draw_bloom(g->pos, g->aura_radius * aura_pulse, {g->color.r, g->color.g, g->color.b, 30});
                            
                            // Visual links to protected glitches
                            std::vector<GameObject*> nearby; 
                            qtree->query(Rect{e->pos.x - 150, e->pos.y - 150, 300, 300}, nearby);
                            for (auto n : nearby) { 
                                if (n != e.get() && (n->type == ObjType::GLITCH_BASIC || n->type == ObjType::GLITCH_DASH || n->type == ObjType::GLITCH_SPLITTER)) { 
                                    gfx.set_color(g->color, 0.4f);
                                    gfx.draw_line(e->pos, n->pos);
                                } 
                            }
                        }
                        if (e->type == ObjType::GLITCH_BOSS) { 
                            float pct = g->stability / g->max_stability; 
                            Vec2 bar_pos = e->pos + Vec2(-75, -110);
                            gfx.draw_filled_rect(bar_pos.x, bar_pos.y, 150, 8, {10, 10, 30, 200});
                            gfx.draw_filled_rect(bar_pos.x, bar_pos.y, 150 * pct, 8, Colors::HARMONY_GOLD);
                            gfx.draw_rect(bar_pos.x, bar_pos.y, 150, 8, Colors::WHITE);
                            gfx.draw_text_centered("RESTORE HARMONY", e->pos + Vec2(0, -130), 16, Colors::NEXUS_WHITE);
                            
                            // Pulsing Shield
                            float shield_r = g->radius * (1.2f + 0.1f * std::sin(stability_timer * 6.0f));
                            gfx.set_color(Colors::VOID_PURPLE, 0.3f);
                            gfx.draw_circle(e->pos, shield_r);
                        }
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
            for (int i = 0; i < 3; ++i) { float rs = std::fmod((stability_timer * 2.0f) + (i * 0.33f), 1.0f), ra = 1.0f - rs, rr = 5.0f + rs * (15.0f + speed_factor * 20.0f); gfx.set_color(Colors::HARMONY_GOLD, ra * 0.6f); gfx.draw_circle(pad_l, rr); gfx.draw_circle(pad_r, rr); }
            float pi = 0.4f + speed_factor * 0.6f; gfx.set_color(Colors::WHITE, pi); gfx.draw_circle(pad_l, 4.0f); gfx.draw_circle(pad_r, 4.0f); gfx.draw_bloom(pad_l, 10.0f * pi, Colors::HARMONY_GOLD); gfx.draw_bloom(pad_r, 10.0f * pi, Colors::HARMONY_GOLD);
            if (combo_count > 1) { 
                std::string title = combo_count > 10 ? "NEXUS GLORY" : (combo_count > 5 ? "HARMONIC RESONANCE" : "COHERENCE"); 
                gfx.draw_text(std::format("{} x{}", title, combo_count), {static_cast<float>(WINDOW_W/2 - 100), 70}, 32, Colors::HARMONY_GOLD); 
                
                // Combo Timer Bar
                float combo_pct = combo_timer / 1.5f;
                gfx.draw_filled_rect(WINDOW_W/2 - 100, 110, 200 * combo_pct, 4, Colors::HARMONY_GOLD);
                gfx.draw_rect(WINDOW_W/2 - 100, 110, 200, 4, {255, 230, 100, 100});
            }
            if (!player->active_virtue.empty()) { gfx.draw_text(std::format("RESONANCE: {}", player->active_virtue), {static_cast<float>(WINDOW_W/2 - 80), 120}, 24, Colors::WHITE); gfx.set_color(Colors::HARMONY_GOLD, player->blessing_timer / 8.0f); gfx.draw_line({static_cast<float>(WINDOW_W/2 - 100), 150}, {static_cast<float>(WINDOW_W/2 + 100), 150}); }
        }

        if (state == State::MENU) {
            gfx.draw_text_centered("LENTEN VIGIL", {static_cast<float>(WINDOW_W/2), 200}, 90, Colors::NEXUS_PURPLE);
            gfx.draw_text_centered("THE DESERT PATH", {static_cast<float>(WINDOW_W/2), 280}, 40, Colors::RESONANCE_GOLD);
            gfx.draw_text_centered("FAST, PRAY, AND RESTORE THE LIGHT", {static_cast<float>(WINDOW_W/2), 400}, 24, Colors::NEXUS_WHITE);
            float pulse = 0.5f + 0.5f * std::sin(stability_timer * 3.0f);
            gfx.draw_text_centered("PRESS SPACE TO BEGIN YOUR VIGIL", {static_cast<float>(WINDOW_W/2), 550}, 24, {Colors::HARMONY_GOLD.r, Colors::HARMONY_GOLD.g, Colors::HARMONY_GOLD.b, (Uint8)(255 * pulse)});
        } else if (state == State::GAME_OVER) {
            gfx.draw_text_centered("SPIRITUAL DESOLATION", {static_cast<float>(WINDOW_W/2), 300}, 75, Colors::VOID_PURPLE);
            gfx.draw_text_centered("THE SHADOWS HAVE GROWN LONG", {static_cast<float>(WINDOW_W/2), 380}, 28, Colors::SHADOW_DARK);
            gfx.draw_text_centered("PRESS SPACE TO SEEK GRACE ANEW", {static_cast<float>(WINDOW_W/2), 550}, 24, Colors::WHITE);
        } else if (state == State::VICTORY) {
            gfx.draw_text_centered("EASTER DAWN APPROACHES", {static_cast<float>(WINDOW_W/2), 250}, 70, Colors::RESONANCE_GOLD);
            gfx.draw_text_centered("THE LIGHT OF CHRIST SHINES", {static_cast<float>(WINDOW_W/2), 380}, 32, Colors::NEXUS_WHITE);
            gfx.draw_text_centered("PRESS SPACE TO CONTINUE THE JOURNEY", {static_cast<float>(WINDOW_W/2), 550}, 26, Colors::HARMONY_GOLD);
        }
        if (state == State::PAUSED) render_pause_menu();
        if (messenger_open) render_messenger();
        if (current_dialog_idx != -1 && !messenger_open) render_player_bubble();
        if (evolution_pending) render_evolution_overlay();
        
        // Post-processing: Scanlines
        gfx.draw_scanlines(WINDOW_W, WINDOW_H, stability_timer);
        
        SDL_RenderPresent(renderer);
    }

    void run() {
        auto last_time = std::chrono::high_resolution_clock::now();
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - last_time).count();
            last_time = now;
            if (dt > 0.05f) { dt = 0.05f; }
            if (hit_stop > 0) { 
                hit_stop -= dt; 
                dt = 0; 
            }
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                if (ev.type == SDL_MOUSEBUTTONDOWN) {
                    if (messenger_open) {
                        if (current_dialog_idx == -1) messenger_open = false;
                    }
                    else { int mx, my; SDL_GetMouseState(&mx, &my); if (mx > WINDOW_W - 120 && my < 120) { messenger_open = true; messenger_scroll = 0; current_dialog_idx = -1; } }
                }
                if (ev.type == SDL_MOUSEWHEEL && messenger_open) { messenger_scroll -= ev.wheel.y * 40.0f; float max_scroll = std::max(0.0f, (static_cast<float>(divine_prose.size()) * 120.0f) - 300.0f); messenger_scroll = std::clamp(messenger_scroll, 0.0f, max_scroll); }
                if (ev.type == SDL_KEYDOWN) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        if (messenger_open) {
                            messenger_open = false;
                        } else if (state == State::PLAYING) {
                            state = State::PAUSED;
                        } else if (state == State::PAUSED) {
                            state = State::PLAYING;
                        } else {
                            running = false;
                        }
                    }
                    if (current_dialog_idx != -1 && !messenger_open) {
                        int reply_idx = -1;
                        if (ev.key.keysym.sym == SDLK_1) reply_idx = 0;
                        if (ev.key.keysym.sym == SDLK_2) reply_idx = 1;
                        if (ev.key.keysym.sym == SDLK_3) reply_idx = 2;
                        
                        if (reply_idx != -1 && reply_idx < (int)active_dialogues[current_dialog_idx].replies.size()) {
                            current_dialog_idx = -1;
                            player->integrity = std::min(player->max_integrity, player->integrity + 5.0f);
                            spawn_text_particle(player->pos, "COMMUNION +5%", Colors::HARMONY_GOLD);
                            if (audio) audio->play_collect_harmony();
                        }
                    }
                    if (state == State::MENU && ev.key.keysym.sym == SDLK_SPACE) state = State::PLAYING;
                    if ((state == State::GAME_OVER || state == State::VICTORY) && ev.key.keysym.sym == SDLK_SPACE) { reset_game(); state = State::PLAYING; }
                    
                    if (evolution_pending) {
                        bool chosen = false;
                        if (ev.key.keysym.sym == SDLK_1) { player->ray_count++; player->ray_spread += 0.05f; if(audio) audio->play_evolve_ray_density(); chosen = true; trigger_divine_voice("BE FRUITFUL AND MULTIPLY."); }
                        if (ev.key.keysym.sym == SDLK_2) { player->harmony_power *= 1.45f; if(audio) audio->play_evolve_harmony_power(); chosen = true; trigger_divine_voice("THY WORD IS A SWORD OF SPIRIT."); }
                        if (ev.key.keysym.sym == SDLK_3) { player->ray_pierce++; player->harmony_rate *= 0.9f; if(audio) audio->play_evolve_resonance_rate(); chosen = true; trigger_divine_voice("STAND FAST IN THE FAITH."); }
                        
                        if (chosen) {
                            evolution_pending = false;
                            spawn_text_particle(player->pos, "ASCENDED", Colors::RESONANCE_GOLD);
                        }
                    }
                }
            }
            update(dt); render();
        }
    }
};