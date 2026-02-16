#pragma once
#include <SDL.h>
#include <vector>
#include <cmath>
#include <numbers>
#include <atomic>
#include <span>
#include <mutex>
#include <random>
#include <algorithm>
#include <iostream>
#include <array>

class AudioEngine {
public:
    struct Voice {
        float frequency;
        float amplitude;
        float phase = 0.0f;
        float phase_increment;
        float duration; 
        float elapsed = 0.0f;
        bool active = false;
        bool is_noise = false;
        float attack = 0.01f;
        float decay = 0.1f;
        float freq_mod = 1.0f; 
        float pan = 0.5f; 
    };

    AudioEngine() {}

    bool start() {
        if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return false;
        }
        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = 44100;
        wanted.format = AUDIO_F32SYS;
        wanted.channels = 2;
        wanted.samples = 1024;
        wanted.callback = AudioCallback;
        wanted.userdata = this;
        device = SDL_OpenAudioDevice(NULL, 0, &wanted, &actual, 0);
        if (device == 0) return false;
        SDL_PauseAudioDevice(device, 0);
        return true;
    }

    ~AudioEngine() { if (device != 0) SDL_CloseAudioDevice(device); }

    void play_randomized_sfx(float base_freq, float base_amp, float dur, bool noise = false, float pan = 0.5f) {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float pitch_var = 1.0f + rnd(-0.12f, 0.12f);
        float amp_var = base_amp * (1.0f + rnd(-0.15f, 0.15f));
        play_sfx_internal(base_freq * pitch_var, amp_var, dur, noise, 1.0f, pan, 0.01f);
    }

    void set_player_state(float x_norm, float speed_norm, bool active, float progress_norm) {
        player_pan = std::clamp(x_norm, 0.0f, 1.0f);
        player_speed = std::clamp(speed_norm, 0.0f, 2.0f);
        target_engine_vol = active ? 1.0f : 0.0f;
        harmony_progress = std::clamp(progress_norm, 0.0f, 1.0f);
    }

    void play_divine_ray() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float p = player_pan.load();
        float b40 = 40.0f;
        play_sfx_internal(528.0f, 0.20f, 0.20f, false, 0.994f, 0.0f, 0.001f);
        play_sfx_internal(528.0f + b40, 0.20f, 0.20f, false, 0.994f, 1.0f, 0.001f);
        play_sfx_internal(1056.0f, 0.12f, 0.10f, false, 0.985f, p, 0.001f);
    }

    void play_collect_harmony() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float p = player_pan.load();
        play_sfx_internal(659.25f, 0.35f, 0.8f, false, 1.0f, p, 0.01f);
        play_sfx_internal(55.0f, 0.45f, 0.5f, false, 1.0f, p, 0.05f); 
    }

    void play_warp_jump() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float b40 = 40.0f;
        jump_duck_env = 1.0f;
        play_sfx_internal(32.0f, 0.80f, 1.5f, false, 0.992f, 0.0f, 0.005f); 
        play_sfx_internal(32.0f + b40, 0.80f, 1.5f, false, 0.992f, 1.0f, 0.005f);
    }

    void play_restoration_sigh() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(27.5f, 0.65f, 1.5f, false, 1.0f, 0.5f, 0.1f);
        play_sfx_internal(528.0f, 0.25f, 1.5f, false, 1.0f, 0.5f, 0.05f);
    }

    void play_impact() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(82.41f, 0.45f, 0.15f, false, 0.99f, 0.5f, 0.005f); 
    }

    void play_evolve_ray_density() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<6; ++i) play_sfx_internal(528.0f * (1.0f + i * 0.5f), 0.10f, 0.6f, false, 1.001f, (float)i/5.0f, 0.02f);
    }

    void play_evolve_harmony_power() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(36.71f, 0.80f, 2.0f, false, 1.0f, 0.5f, 0.2f);
    }

    void play_evolve_resonance_rate() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<10; ++i) play_sfx_internal(800.0f + i * 400.0f, 0.05f, 0.3f, false, 0.999f, (float)i/9.0f, 0.01f);
    }

    void play_glitch_spawn() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(55.0f, 0.25f, 1.5f, false, 0.9998f, 0.5f, 0.01f);
    }
    
    void play_dissipate() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(432.0f, 0.02f, 0.1f, false, 1.0f, 0.5f, 0.02f); 
    }

    void set_binaural(float carrier, float beat, float amp) {
        target_carrier = carrier;
        target_beat = beat;
        target_amp = amp;
    }

    struct MusicState { bool is_drop; float intensity; };
    MusicState get_music_state() { return { current_movement == THE_NEXUS || current_movement == RESOLUTION, current_intensity.load() }; }

private:
    void play_sfx_internal(float freq, float amp, float dur, bool noise, float f_mod, float pan, float attack) {
        for (auto& v : sfx_voices) {
            if (!v.active) {
                v.frequency = freq; v.amplitude = amp; v.phase = 0.0f;
                v.phase_increment = (freq * 2.0f * std::numbers::pi_v<float>) / actual.freq;
                v.duration = dur; v.elapsed = 0.0f; v.active = true; v.is_noise = noise;
                v.freq_mod = f_mod; v.pan = std::clamp(pan, 0.0f, 1.0f);
                v.attack = std::max(0.001f, attack); v.decay = dur * 0.8f;
                break;
            }
        }
    }

    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec actual;
    std::mutex mtx;
    
    std::atomic<float> target_carrier{432.0f}; 
    std::atomic<float> target_beat{40.0f}; 
    std::atomic<float> target_amp{0.12f};
    std::atomic<float> player_pan{0.5f};
    std::atomic<float> player_speed{0.0f};
    std::atomic<float> target_engine_vol{0.0f};
    std::atomic<float> current_intensity{0.0f};
    std::atomic<float> harmony_progress{0.0f};

    float current_carrier = 432.0f;
    float current_beat = 40.0f;
    float current_amp = 0.12f; 
    float current_engine_vol = 0.0f;
    
    float bpm = 174.0f; 
    float step_timer = 0.0f;
    uint64_t global_step = 0;
    uint64_t global_bar = 0;
    
    enum Movement { GENESIS, AWAKENING, THE_NEXUS, TRANSCENDENCE, RESOLUTION };
    Movement current_movement = GENESIS;

    float kick_phase_l = 0.0f, kick_phase_r = 0.0f, kick_env = 0.0f;
    float snare_tone_l = 0.0f, snare_tone_r = 0.0f, snare_pop_phase = 0.0f, snare_env = 0.0f;
    float hat_env = 0.0f, hat_phases[6] = {0};
    float reese_phases_l[4] = {0}, reese_phases_r[4] = {0}, bass_amp = 0.0f, bass_freq = 108.0f;
    float lead_phase_l = 0.0f, lead_phase_r = 0.0f, lead_amp = 0.0f, lead_freq = 432.0f;
    float bin_phase_l = 0.0f, bin_phase_r = 0.0f;
    
    float sidechain_duck = 1.0f; 
    float jump_duck_env = 0.0f; 
    float swoosh_phase_l = 0.0f, swoosh_phase_r = 0.0f;
    float musical_sub_phase_l = 0.0f, musical_sub_phase_r = 0.0f;

    static constexpr int MAX_SFX = 96;
    Voice sfx_voices[MAX_SFX];
    std::mt19937 noise_gen{std::random_device{}()};
    std::uniform_real_distribution<float> noise_dist{-1.0f, 1.0f};

    float get_saw(float& p, float f, float sr) {
        p += (f * 2.0f * std::numbers::pi_v<float>) / sr;
        if (p > std::numbers::pi_v<float>) p -= 2.0f * std::numbers::pi_v<float>;
        return p / std::numbers::pi_v<float>;
    }

    static void AudioCallback(void* userdata, Uint8* stream, int len) {
        auto* engine = static_cast<AudioEngine*>(userdata);
        std::span<float> buffer(reinterpret_cast<float*>(stream), len / sizeof(float));
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        engine->generate(buffer);
    }

    float soft_limit(float x) {
        float threshold = 0.85f;
        if (std::abs(x) < threshold) return x;
        if (x > 0) return threshold + (1.0f - threshold) * std::tanh((x - threshold) / (1.0f - threshold));
        return -threshold - (1.0f - threshold) * std::tanh((-x - threshold) / (1.0f - threshold));
    }

    void generate(std::span<float> buffer) {
        std::lock_guard<std::mutex> lock(mtx);
        float sample_rate = static_cast<float>(actual.freq);
        if (sample_rate <= 0) return;
        
        current_carrier = std::lerp(current_carrier, target_carrier.load(), 0.005f);
        current_beat = std::lerp(current_beat, target_beat.load(), 0.005f);
        current_amp = std::lerp(current_amp, target_amp.load(), 0.005f);
        current_engine_vol = std::lerp(current_engine_vol, target_engine_vol.load(), 0.01f);

        float pan = player_pan.load();
        float speed = player_speed.load();
        float step_dur = 60.0f / (bpm * 4.0f);

        for (size_t i = 0; i < buffer.size(); i += 2) {
            step_timer += 1.0f / sample_rate;
            jump_duck_env = std::lerp(jump_duck_env, 0.0f, 0.001f);
            
            if (step_timer >= step_dur) {
                step_timer -= step_dur;
                global_step++;
                if (global_step % 16 == 0) global_bar++;

                if (global_bar < 64) current_movement = GENESIS;
                else if (global_bar < 128) current_movement = AWAKENING;
                else if (global_bar < 256) current_movement = THE_NEXUS;
                else if (global_bar < 320) current_movement = TRANSCENDENCE;
                else if (global_bar < 384) current_movement = RESOLUTION;
                else { global_bar = 0; }

                if (current_movement >= GENESIS) {
                    // INDUSTRIAL KICK (40Hz)
                    bool k_hit = (global_step % 4 == 0);
                    if (current_movement == THE_NEXUS && (global_step % 16 == 6 || global_step % 16 == 14)) k_hit = true;
                    if (k_hit) { kick_env = 1.0f; kick_phase_l = kick_phase_r = 0.0f; sidechain_duck = 0.4f; }

                    // VIGILANCE SNARE (Ghost notes for alertness)
                    bool s_hit = (global_step % 16 == 4 || global_step % 16 == 12);
                    bool ghost = (current_movement >= AWAKENING && (global_step % 16 == 7 || global_step % 16 == 15));
                    if (s_hit || ghost) {
                        snare_env = ghost ? 0.35f : 1.0f;
                        snare_tone_l = snare_tone_r = snare_pop_phase = 0.0f;
                        if (!ghost) sidechain_duck = 0.7f;
                    }

                    if (global_step % 2 == 1) hat_env = 0.45f;

                    // HIVE REESE BASS (Focus foundation)
                    bool b_hit = (global_step % 8 == 0 || (current_movement == THE_NEXUS && global_step % 8 == 3));
                    if (b_hit) {
                        bass_amp = 0.95f; 
                        std::array<float, 4> progression = {1.0f, 0.75f, 1.25f, 0.9f};
                        bass_freq = current_carrier * 0.125f * progression[(global_bar / 4) % 4];
                    }

                    // DIVINE LEAD (Perfect Fifths - No cartoon noodling)
                    if (global_step % 8 == 0) {
                        std::array<float, 3> hive = {1.0f, 1.5f, 2.0f};
                        lead_freq = current_carrier * hive[(global_step/8) % 3];
                        lead_amp = 0.45f;
                    }
                }
            }

            sidechain_duck = std::lerp(sidechain_duck, 1.0f, 0.005f);
            float m_duck = sidechain_duck * (1.0f - jump_duck_env * 0.7f);

            float out_l = 0.0f, out_r = 0.0f;
            float b_gamma = 40.0f; // GAMMA FOCUS
            float b_beta = 20.0f;  // BETA ALERTNESS
            float p_l = std::cos(pan * (std::numbers::pi_v<float> * 0.5f));
            float p_r = std::sin(pan * (std::numbers::pi_v<float> * 0.5f));

            current_intensity.store(std::lerp(current_intensity.load(), kick_env * 0.5f + bass_amp * 0.5f, 0.001f));

            // --- 1. SILENT SOVEREIGN (VOID-STREAM BINAURAL) ---
            float s_f = 432.0f * (1.0f + speed * 0.15f);
            swoosh_phase_l += (s_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            swoosh_phase_r += ((s_f + b_beta) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            float s_amp = 0.08f * speed * current_engine_vol * m_duck;
            out_l += std::sin(swoosh_phase_l) * s_amp * p_l;
            out_r += std::sin(swoosh_phase_r) * s_amp * p_r;

            // --- 2. THE HIVE SYMPHONY (BINAURAL ENTRAINMENT) ---
            // A. PERSISTENT SUB-FLOOR (GAMMA 40HZ)
            float sub_f = current_carrier * 0.125f; 
            musical_sub_phase_l += (sub_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            musical_sub_phase_r += ((sub_f + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            out_l += std::sin(musical_sub_phase_l) * 0.35f * m_duck;
            out_r += std::sin(musical_sub_phase_r) * 0.35f * m_duck;

            // B. DRUMS
            if (kick_env > 0.001f) {
                float k_f = 40.0f + 150.0f * (kick_env * kick_env);
                out_l += std::sin(kick_phase_l) * kick_env * 0.85f;
                out_r += std::sin(kick_phase_r) * kick_env * 0.85f;
                kick_phase_l += (k_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_phase_r += ((k_f + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_env *= 0.9994f;
            }
            if (snare_env > 0.001f) {
                float p_f = 180.0f + 600.0f * (snare_env * snare_env);
                snare_pop_phase += (p_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                float pop = std::sin(snare_pop_phase) * snare_env * 0.45f;
                snare_tone_l += (180.0f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                snare_tone_r += ((180.0f + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                float rattle = noise_dist(noise_gen) * snare_env * 0.45f;
                float s_sig_l = std::tanh((pop + std::sin(snare_tone_l) * 0.25f + rattle) * 1.6f);
                float s_sig_r = std::tanh((pop + std::sin(snare_tone_r) * 0.25f + rattle) * 1.6f);
                out_l += s_sig_l * 0.5f; out_r += s_sig_r * 0.5f;
                snare_env *= 0.9988f; 
            }
            if (hat_env > 0.001f) {
                float h = noise_dist(noise_gen) * hat_env * 0.08f;
                out_l += h; out_r += h; hat_env *= 0.997f;
            }

            // C. HIVE REESE (Detuned Sawtooths)
            if (bass_amp > 0.001f) {
                float b_l = 0, b_r = 0;
                std::array<float, 4> detunes = {1.0f, 1.002f, 0.998f, 1.005f};
                for(int j=0; j<4; ++j) {
                    b_l += get_saw(reese_phases_l[j], bass_freq * detunes[j], sample_rate);
                    b_r += get_saw(reese_phases_r[j], (bass_freq + b_gamma) * detunes[j], sample_rate);
                }
                out_l += std::tanh(b_l * 0.25f) * bass_amp * 0.35f * m_duck;
                out_r += std::tanh(b_r * 0.25f) * bass_amp * 0.35f * m_duck;
                bass_amp *= 0.99985f;
            }

            // D. DIVINE LEAD
            if (lead_amp > 0.001f) {
                lead_phase_l += (lead_freq * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                lead_phase_r += ((lead_freq + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                out_l += std::sin(lead_phase_l) * lead_amp * 0.12f;
                out_r += std::sin(lead_phase_r) * lead_amp * 0.12f;
                lead_amp *= 0.99975f;
            }

            // --- 3. SFX ---
            for (auto& vox : sfx_voices) {
                if (vox.active) {
                    float s = vox.is_noise ? (noise_dist(noise_gen) * 0.4f) : std::sin(vox.phase);
                    s *= vox.amplitude;
                    if (vox.elapsed < vox.attack) s *= (vox.elapsed / vox.attack);
                    else if (vox.elapsed > vox.duration - vox.decay) { float rem = (vox.duration - vox.elapsed) / vox.decay; s *= std::max(0.0f, rem); }
                    out_l += s * (1.0f - vox.pan); out_r += s * vox.pan;
                    if (!vox.is_noise) { vox.phase += vox.phase_increment; vox.phase_increment *= vox.freq_mod; }
                    vox.elapsed += 1.0f / sample_rate;
                    if (vox.elapsed >= vox.duration) vox.active = false;
                }
            }

            buffer[i] = soft_limit(out_l);
            buffer[i + 1] = soft_limit(out_r);
        }
    }
};
