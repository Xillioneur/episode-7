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
        bool has_echo = false;
        float freq_mod = 1.0f; 
        float pan = 0.5f; 
    };

    AudioEngine() {
        std::fill(delay_buffer.begin(), delay_buffer.end(), 0.0f);
    }

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

    void play_sfx(float freq, float amp, float dur, bool noise = false, float f_mod = 1.0f, float pan = 0.5f, float attack = 0.01f) {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(freq, amp, dur, noise, f_mod, pan, attack);
    }

    void set_player_state(float x_norm, float speed_norm, bool active) {
        player_pan = std::clamp(x_norm, 0.0f, 1.0f);
        player_speed = std::clamp(speed_norm, 0.0f, 2.0f);
        target_engine_vol = active ? 1.0f : 0.0f;
    }

    void play_divine_ray() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(528.0f, 0.05f, 0.15f, false, 0.999f, player_pan.load(), 0.002f);
        play_sfx_internal(1056.0f, 0.02f, 0.1f, false, 1.0f, player_pan.load(), 0.001f);
    }

    void play_collect_harmony() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float phi = 1.618f;
        play_sfx_internal(659.25f, 0.08f, 0.5f, false, 1.0f, player_pan.load(), 0.001f);
        play_sfx_internal(659.25f * phi, 0.04f, 0.7f, false, 1.0f, player_pan.load(), 0.005f);
    }

    void play_evolve_ray_density() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<5; ++i) play_sfx_internal(440.0f * (1.0f + i * 0.5f), 0.03f, 1.0f, false, 1.0f, 0.5f, 0.1f);
    }

    void play_evolve_harmony_power() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(65.41f, 0.15f, 2.0f, false, 1.0f, 0.5f, 0.2f);
        play_sfx_internal(130.81f, 0.1f, 1.8f, false, 1.0f, 0.5f, 0.15f);
    }

    void play_evolve_resonance_rate() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<12; ++i) play_sfx_internal(880.0f * (1.0f + (float)i * 0.125f), 0.02f, 0.4f, false, 1.0f, (float)i/11.0f, 0.005f);
    }

    void play_glitch_spawn() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(55.0f, 0.15f, 1.5f, false, 0.9998f, 0.5f, 0.01f);
        play_sfx_internal(110.0f, 0.08f, 1.2f, false, 0.9998f, 0.5f, 0.02f);
    }

    void play_impact() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(2200.0f + rnd(-100, 100), 0.06f, 0.05f, false, 0.99f, rnd(0.3f, 0.7f), 0.001f); 
    }
    
    void play_dissipate() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(4000.0f, 0.01f, 0.2f, true, 1.0f, 0.5f, 0.05f); 
    }

    void set_binaural(float carrier, float beat, float amp) {
        target_carrier = carrier;
        target_beat = beat;
        target_amp = amp;
    }

private:
    void play_sfx_internal(float freq, float amp, float dur, bool noise, float f_mod, float pan, float attack) {
        for (auto& v : sfx_voices) {
            if (!v.active) {
                v.frequency = freq;
                v.amplitude = amp;
                v.phase = 0.0f;
                v.phase_increment = (freq * 2.0f * std::numbers::pi_v<float>) / actual.freq;
                v.duration = dur;
                v.elapsed = 0.0f;
                v.active = true;
                v.is_noise = noise;
                v.freq_mod = f_mod;
                v.pan = std::clamp(pan, 0.0f, 1.0f);
                v.attack = std::max(0.001f, attack); 
                v.decay = dur * 0.8f;
                break;
            }
        }
    }

    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec actual;
    std::mutex mtx;
    
    std::atomic<float> target_carrier{432.0f}; 
    std::atomic<float> target_beat{10.0f}; 
    std::atomic<float> target_amp{0.15f};
    std::atomic<float> player_pan{0.5f};
    std::atomic<float> player_speed{0.0f};
    std::atomic<float> target_engine_vol{0.0f};

    float current_carrier = 432.0f;
    float current_beat = 10.0f;
    float current_amp = 0.12f; 
    float current_engine_vol = 0.0f;
    
    float bpm = 174.0f; 
    float step_timer = 0.0f;
    uint64_t global_step = 0;
    uint64_t global_bar = 0;
    
    enum Movement { GENESIS, AWAKENING, THE_NEXUS, TRANSCENDENCE, RESOLUTION };
    Movement current_movement = GENESIS;

    float kick_amp = 0.0f, kick_phase = 0.0f;
    float snare_amp = 0.0f, hat_amp = 0.0f;
    float bass_phase = 0.0f, bass_amp = 0.0f, bass_freq = 108.0f;
    float lead_phase = 0.0f, lead_mod_phase = 0.0f, lead_amp = 0.0f, lead_freq = 432.0f;
    float bin_phase_l = 0.0f, bin_phase_r = 0.0f;
    
    // Clean defined phases for the Resonance Stack
    float engine_phases[3] = {0, 0, 0};
    float gravity_throb_phase = 0.0f;

    static constexpr int DELAY_SIZE = 44100 * 2;
    std::array<float, DELAY_SIZE> delay_buffer;
    int delay_ptr = 0;

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
        float step_dur = 60.0f / (bpm * 4.0f);

        for (size_t i = 0; i < buffer.size(); i += 2) {
            step_timer += 1.0f / sample_rate;
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

                if (current_movement == GENESIS) {
                    if (global_step % 4 == 0) { kick_amp = 1.0f; kick_phase = 0.0f; } 
                    if (global_step % 8 == 4) snare_amp = 0.5f;
                    bass_freq = current_carrier * chord * 0.25f;
                    if (global_step % 16 == 0) bass_amp = 0.6f;
                }
                if (current_movement == AWAKENING) {
                    if (global_step % 4 == 0) { kick_amp = 1.0f; kick_phase = 0.0f; }
                    if (global_step % 8 == 4) snare_amp = 0.8f;
                    if (global_step % 2 == 1) hat_amp = 0.3f;
                    if (global_step % 12 == 0) { lead_freq = current_carrier * chord * 2.0f; lead_amp = 0.3f; }
                }
                if (current_movement == THE_NEXUS) {
                    if (global_step % 16 == 0 || global_step % 16 == 6 || global_step % 16 == 9) { kick_amp = 1.0f; kick_phase = 0.0f; }
                    if (global_step % 16 == 4 || global_step % 16 == 10) snare_amp = 1.0f;
                    if (global_step % 2 == 1) hat_amp = 0.4f;
                    bass_amp = 0.8f;
                    bass_freq = current_carrier * chord * 0.25f;
                    if (global_step % 4 == 0) {
                        lead_freq = current_carrier * chord * (1.0f + (float)(global_step % 5) * 0.25f);
                        lead_amp = 0.4f;
                    }
                }
                if (current_movement == TRANSCENDENCE) {
                    kick_amp *= 0.9f; snare_amp *= 0.9f;
                    bass_amp = 0.4f;
                    if (global_step % 3 == 0) { lead_freq = current_carrier * chord * 4.0f; lead_amp = 0.2f; }
                }
                if (current_movement == RESOLUTION) {
                    if (global_step % 4 == 0) { kick_amp = 1.0f; kick_phase = 0.0f; }
                    snare_amp = 0.8f; bass_amp = 0.9f; lead_amp = 0.5f;
                }
            }

            float out_l = 0.0f, out_r = 0.0f;

            // --- 1. THE RESONANCE STACK (Defined Frequencies, No Noise) ---
            // A. Gravity Throb (Slow LFO for life)
            float throb = 0.85f + 0.15f * std::sin(gravity_throb_phase);
            gravity_throb_phase += (4.0f * 2.0f * std::numbers::pi_v<float>) / sample_rate;

            // B. Three Defined Tiers: Sub (54Hz), Core (432Hz), Crown (528Hz)
            std::array<float, 3> engine_freqs = { 54.0f, 432.0f, 528.0f };
            std::array<float, 3> engine_amps = { 0.15f, 0.08f, 0.04f };
            
            float engine_total = 0.0f;
            for(int j=0; j<3; ++j) {
                float f = engine_freqs[j] * (1.0f + speed * 0.05f); // Pitch shift with speed
                engine_total += std::sin(engine_phases[j]) * engine_amps[j] * current_engine_vol * throb;
                engine_phases[j] += (f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            }

            out_l += engine_total * (1.0f - pan);
            out_r += engine_total * pan;

            // --- 2. THE DIVINE BINAURAL FIELD (Defined Pure Sines) ---
            float dynamic_beat = 14.0f + (speed * 26.0f); 
            float sidechain = 1.0f - (kick_amp * 0.6f);
            float root = 432.0f;
            float offset = root + dynamic_beat;
            
            float spatial_l = std::clamp(1.2f - pan, 0.5f, 1.0f);
            float spatial_r = std::clamp(0.2f + pan, 0.5f, 1.0f);
            
            float bin_l = std::sin(bin_phase_l) * current_amp * sidechain * 0.25f * spatial_l * current_engine_vol;
            float bin_r = std::sin(bin_phase_r) * current_amp * sidechain * 0.25f * spatial_r * current_engine_vol;
            out_l += bin_l; out_r += bin_r;
            
            bin_phase_l += (root * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            bin_phase_r += (offset * 2.0f * std::numbers::pi_v<float>) / sample_rate;

            // --- 3. THE 8.8 MINUTE SYMPHONY ---
            if (kick_amp > 0) {
                float s = std::sin(kick_phase) * kick_amp * 0.5f;
                out_l += s; out_r += s;
                kick_phase += (140.0f * kick_amp * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_amp *= 0.9991f;
            }
            if (snare_amp > 0) {
                float s = noise_dist(noise_gen) * snare_amp * 0.25f;
                out_l += s; out_r += s; snare_amp *= 0.9994f;
            }
            if (hat_amp > 0) {
                float s = noise_dist(noise_gen) * hat_amp * 0.15f;
                out_l += s; out_r += s; hat_amp *= 0.9980f;
            }
            if (bass_amp > 0) {
                float s1 = divine_wave(bass_phase, 3);
                out_l += s1 * bass_amp * 0.15f; out_r += s1 * bass_amp * 0.15f;
                bass_phase += (bass_freq * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                bass_amp *= 0.9998f;
            }
            if (lead_amp > 0) {
                float s = std::sin(lead_phase);
                out_l += s * lead_amp * 0.08f; out_r += s * lead_amp * 0.08f;
                lead_phase += (lead_freq * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                lead_amp *= 0.9997f;
            }

            for (auto& vox : sfx_voices) {
                if (vox.active) {
                    float s = vox.is_noise ? (noise_dist(noise_gen) * 0.4f) : std::sin(vox.phase);
                    s *= vox.amplitude;
                    if (vox.elapsed < vox.attack) s *= (vox.elapsed / vox.attack);
                    else if (vox.elapsed > vox.duration - vox.decay) {
                        float rem = (vox.duration - vox.elapsed) / vox.decay;
                        s *= std::max(0.0f, rem);
                    }
                    out_l += s * (1.0f - vox.pan);
                    out_r += s * vox.pan;
                    if (!vox.is_noise) { vox.phase += vox.phase_increment; vox.phase_increment *= vox.freq_mod; }
                    vox.elapsed += 1.0f / sample_rate;
                    if (vox.elapsed >= vox.duration) vox.active = false;
                }
            }

            auto wrap = [](float& p) { if (p > 2.0f * std::numbers::pi_v<float>) p -= 2.0f * std::numbers::pi_v<float>; };
            wrap(kick_phase); wrap(bass_phase); wrap(lead_phase); wrap(bin_phase_l); wrap(bin_phase_r); 
            wrap(gravity_throb_phase);
            for(int j=0; j<3; ++j) wrap(engine_phases[j]);

            buffer[i] = std::clamp(out_l, -1.0f, 1.0f);
            buffer[i + 1] = std::clamp(out_r, -1.0f, 1.0f);
        }
    }
};
