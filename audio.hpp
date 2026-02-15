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
        play_sfx_internal(528.0f, 0.04f, 0.12f, false, 0.999f, p, 0.002f);
        play_sfx_internal(1056.0f, 0.015f, 0.08f, false, 1.0f, p, 0.001f);
    }

    void play_collect_harmony() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float p = player_pan.load();
        play_sfx_internal(659.25f, 0.08f, 0.6f, false, 1.0f, p, 0.001f);
        play_sfx_internal(55.0f, 0.12f, 0.4f, false, 1.0f, p, 0.05f);
    }

    void play_warp_jump() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float b40 = 40.0f;
        // DIMENSIONAL ANCHOR JUMP (Pure Sub-Bass Event)
        // Layered sine stack for physically massive but pristine punch
        play_sfx_internal(32.0f, 0.6f, 1.5f, false, 0.992f, 0.0f, 0.005f); 
        play_sfx_internal(32.0f + b40, 0.6f, 1.5f, false, 0.992f, 1.0f, 0.005f);
        play_sfx_internal(64.0f, 0.4f, 1.0f, false, 0.99f, 0.0f, 0.002f);
        play_sfx_internal(64.0f + b40, 0.4f, 1.0f, false, 0.99f, 1.0f, 0.002f);
        play_sfx_internal(128.0f, 0.2f, 0.6f, false, 0.98f, 0.5f, 0.001f);
    }

    void play_restoration_sigh() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(27.5f, 0.35f, 1.5f, false, 1.0f, 0.5f, 0.1f);
        play_sfx_internal(55.0f, 0.25f, 1.2f, false, 1.0f, 0.5f, 0.05f);
    }

    void play_impact() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(110.0f, 0.15f, 0.1f, false, 0.99f, 0.5f, 0.002f); 
        play_sfx_internal(741.0f, 0.04f, 0.05f, false, 0.99f, 0.5f, 0.002f); 
    }

    void play_evolve_ray_density() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<6; ++i) play_sfx_internal(528.0f * (1.0f + i * 0.25f), 0.04f, 0.6f, false, 1.001f, (float)i/5.0f, 0.02f);
    }

    void play_evolve_harmony_power() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(36.71f, 0.4f, 2.0f, false, 1.0f, 0.5f, 0.2f);
        play_sfx_internal(74.0f, 0.25f, 1.5f, false, 1.0f, 0.5f, 0.1f);
    }

    void play_evolve_resonance_rate() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<10; ++i) play_sfx_internal(800.0f + i * 200.0f, 0.03f, 0.3f, false, 0.999f, (float)i/9.0f, 0.01f);
    }

    void play_glitch_spawn() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(55.0f, 0.15f, 1.5f, false, 0.9998f, 0.5f, 0.01f);
        play_sfx_internal(110.0f, 0.12f, 1.2f, false, 0.9998f, 0.5f, 0.02f);
    }
    
    void play_dissipate() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(2112.0f, 0.01f, 0.1f, true, 1.0f, 0.5f, 0.02f); 
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
    std::atomic<float> target_amp{0.15f};
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
    float snare_tone_l = 0.0f, snare_tone_r = 0.0f, snare_env = 0.0f;
    float hat_env = 0.0f, hat_phases[6] = {0};
    float bass_phase_l = 0.0f, bass_phase_r = 0.0f, bass_amp = 0.0f, bass_freq = 108.0f;
    float lead_phase_l = 0.0f, lead_phase_r = 0.0f, lead_amp = 0.0f, lead_freq = 432.0f;
    float bin_phase_l = 0.0f, bin_phase_r = 0.0f;
    
    // --- DIMENSIONAL OVERDRIVE ENGINE STATE ---
    float reactor_phases_l[12] = {0};
    float reactor_phases_r[12] = {0};
    float hull_resonance_phase = 0.0f;
    float variance_timer = 0.0f;

    static constexpr int MAX_SFX = 96;
    Voice sfx_voices[MAX_SFX];
    std::mt19937 noise_gen{std::random_device{}()};
    std::uniform_real_distribution<float> noise_dist{-1.0f, 1.0f};

    float divine_wave(float p, int harmonics = 4) {
        float s = 0.0f;
        for (int i = 1; i <= harmonics; ++i) s += std::sin(p * (float)i) / (float)i;
        return s * 0.6f;
    }

    static void AudioCallback(void* userdata, Uint8* stream, int len) {
        auto* engine = static_cast<AudioEngine*>(userdata);
        std::span<float> buffer(reinterpret_cast<float*>(stream), len / sizeof(float));
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        engine->generate(buffer);
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
        float prog = harmony_progress.load();
        float step_dur = 60.0f / (bpm * 4.0f);

        for (size_t i = 0; i < buffer.size(); i += 2) {
            step_timer += 1.0f / sample_rate;
            variance_timer += 1.0f / sample_rate;
            
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

                std::array<float, 4> chords = {1.0f, 0.84f, 1.2f, 0.94f}; 
                float chord = chords[(global_bar / 8) % 4];

                if (current_movement >= GENESIS) {
                    if (global_step % 4 == 0) { kick_env = 1.0f; kick_phase_l = kick_phase_r = 0.0f; }
                    if (global_step % 16 == 4 || global_step % 16 == 12) { snare_env = 1.0f; snare_tone_l = snare_tone_r = 0.0f; }
                    if (global_step % 2 == 1) hat_env = 0.4f;
                    if (global_step % 8 == 0) { bass_amp = 0.85f; bass_freq = current_carrier * chord * 0.25f; }
                    if (global_step % 4 == 0) {
                        std::array<float, 7> scale = {1.0f, 1.125f, 1.25f, 1.333f, 1.5f, 1.666f, 1.875f};
                        lead_freq = current_carrier * chord * scale[global_step % 7];
                        lead_amp = 0.45f;
                    }
                }
            }

            float out_l = 0.0f, out_r = 0.0f;
            float b40 = 40.0f; float b20 = 20.0f;
            float pan_l = std::cos(pan * (std::numbers::pi_v<float> * 0.5f));
            float pan_r = std::sin(pan * (std::numbers::pi_v<float> * 0.5f));

            current_intensity.store(std::lerp(current_intensity.load(), kick_env * 0.5f + bass_amp * 0.5f, 0.001f));

            // --- 1. STARSHIP OVERDRIVE (HARMONIC EXPANSION) ---
            // A. Physical Hull Vibrations (Step 4 Randomization)
            float hull_f = 15.0f + 30.0f * std::sin(variance_timer * 0.1f);
            hull_resonance_phase += (hull_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            float vibration = 0.9f + 0.1f * std::sin(hull_resonance_phase);

            // B. Expanding Solfeggio Stack (Accelerating Timbre)
            // As speed increases, more harmonics emerge (Doppler-like expansion)
            std::array<float, 12> r_freqs = { 
                27.0f, 54.0f, 81.0f, 108.0f,   // Sub Foundations
                162.0f, 216.0f, 324.0f, 432.0f, // Core Power
                528.0f, 648.0f, 864.0f, 1056.0f // High-Velocity Shimmers
            };
            std::array<float, 12> r_amps = { 
                0.50f, 0.35f, 0.20f, 0.15f, 
                0.10f, 0.08f, 0.06f, 0.04f, 
                0.03f, 0.02f, 0.015f, 0.01f 
            };

            for(int j=0; j<12; ++j) {
                // Harmonic Emergence: higher layers only loud at high speed
                float speed_threshold = (float)j / 12.0f;
                float dynamic_amp = r_amps[j];
                if (j >= 4) dynamic_amp *= std::clamp((speed - speed_threshold) * 4.0f, 0.0f, 1.0f);

                // DYNAMIC REVING: frequency shifts up with speed
                float f_l = r_freqs[j] * (1.0f + speed * 0.35f); 
                float f_r = f_l + ((j < 4) ? b20 : b40);
                
                out_l += std::sin(reactor_phases_l[j]) * dynamic_amp * current_engine_vol * vibration * pan_l;
                out_r += std::sin(reactor_phases_r[j]) * dynamic_amp * current_engine_vol * vibration * pan_r;
                
                reactor_phases_l[j] += (f_l * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                reactor_phases_r[j] += (f_r * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            }

            // --- 2. GENERATIVE MUSIC ---
            auto apply_b = [&](float f, float amp, float& p_l, float& p_r, bool organ = false) {
                float mf_l = f; float mf_r = f + b40;
                float s_l = organ ? divine_wave(p_l, 3 + (int)(prog*4)) : std::sin(p_l);
                float s_r = organ ? divine_wave(p_r, 3 + (int)(prog*4)) : std::sin(p_r);
                out_l += s_l * amp; out_r += s_r * amp;
                p_l += (mf_l * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                p_r += (mf_r * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            };

            if (kick_env > 0.001f) {
                float k_f = 45.0f + 250.0f * (kick_env * kick_env);
                out_l += std::sin(kick_phase_l) * kick_env * 0.6f;
                out_r += std::sin(kick_phase_r) * kick_env * 0.6f;
                kick_phase_l += (k_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_phase_r += ((k_f + b40) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_env *= 0.9993f;
            }
            if (snare_env > 0.001f) {
                float s_l = (std::sin(snare_tone_l) * 0.4f + noise_dist(noise_gen) * 0.6f);
                float s_r = (std::sin(snare_tone_r) * 0.4f + noise_dist(noise_gen) * 0.6f);
                out_l += s_l * snare_env * 0.35f; out_r += s_r * snare_env * 0.35f;
                snare_tone_l += (220.0f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                snare_tone_r += ((220.0f + b40) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                snare_env *= 0.9991f;
            }
            if (hat_env > 0.001f) {
                float h = noise_dist(noise_gen) * hat_env * 0.08f;
                out_l += h; out_r += h; hat_env *= 0.997f;
            }
            if (bass_amp > 0.001f) { apply_b(bass_freq, bass_amp * 0.12f, bass_phase_l, bass_phase_r, true); bass_amp *= 0.99985f; }
            if (lead_amp > 0.001f) { apply_b(lead_freq, lead_amp * 0.06f, lead_phase_l, lead_phase_r); lead_amp *= 0.99975f; }

            out_l += std::sin(bin_phase_l) * current_amp * 0.25f * (1.0f - kick_env*0.5f);
            out_r += std::sin(bin_phase_r) * current_amp * 0.25f * (1.0f - kick_env*0.5f);
            bin_phase_l += (432.0f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            bin_phase_r += ((432.0f + b40) * 2.0f * std::numbers::pi_v<float>) / sample_rate;

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

            buffer[i] = std::clamp(out_l, -1.0f, 1.0f);
            buffer[i + 1] = std::clamp(out_r, -1.0f, 1.0f);
        }
    }
};