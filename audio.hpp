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
        play_sfx_internal(1500.0f, 0.35f, 0.10f, false, 0.98f, p, 0.001f);
        play_sfx_internal(528.0f, 0.45f, 0.3f, false, 0.995f, 0.0f, 0.002f);
        play_sfx_internal(528.0f + b40, 0.45f, 0.3f, false, 0.995f, 1.0f, 0.002f);
        play_sfx_internal(88.0f, 0.55f, 0.2f, false, 0.99f, p, 0.005f);
    }

    void play_collect_harmony() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float p = player_pan.load();
        float b8 = 8.0f;
        play_sfx_internal(216.0f, 0.25f, 0.8f, false, 1.0f, 0.0f, 0.01f);
        play_sfx_internal(216.0f + b8, 0.25f, 0.8f, false, 1.0f, 1.0f, 0.01f);
        play_sfx_internal(108.0f, 0.30f, 0.4f, false, 1.0f, p, 0.02f);
    }

    void play_warp_jump() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float b40 = 40.0f;
        play_sfx_internal(32.0f, 0.45f, 1.2f, false, 0.992f, 0.0f, 0.005f); 
        play_sfx_internal(32.0f + b40, 0.45f, 1.2f, false, 0.992f, 1.0f, 0.005f);
    }

    void play_restoration_sigh() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(108.0f, 0.30f, 3.0f, false, 1.0f, 0.5f, 0.2f);
        play_sfx_internal(216.0f, 0.25f, 2.5f, false, 1.0f, 0.5f, 0.1f);
        play_sfx_internal(54.0f, 0.40f, 4.0f, false, 1.0f, 0.5f, 0.5f);
    }

    void play_harmonic_combo(int combo) {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        static const float scale[] = { 396.0f, 417.0f, 528.0f, 639.0f, 741.0f, 852.0f };
        float freq = scale[combo % 6];
        float amp = 0.25f + std::min(0.2f, combo * 0.03f);
        play_sfx_internal(freq, amp, 0.4f, false, 1.0f, 0.5f, 0.01f);
    }

    void play_impact() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        float b8 = 8.0f;
        play_sfx_internal(660.0f, 0.55f, 0.2f, false, 0.99f, 0.0f, 0.005f);
        play_sfx_internal(660.0f + b8, 0.55f, 0.2f, false, 0.99f, 1.0f, 0.005f);
        play_sfx_internal(132.0f, 0.65f, 0.3f, false, 0.99f, 0.5f, 0.005f);
    }

    void play_evolve_ray_density() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<3; ++i) play_sfx_internal(216.0f * (1.0f + i * 0.5f), 0.10f, 0.6f, false, 1.001f, (float)i/2.0f, 0.05f);
    }

    void play_evolve_harmony_power() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(36.71f, 0.35f, 1.2f, false, 1.0f, 0.5f, 0.2f);
        play_sfx_internal(73.42f, 0.25f, 1.0f, false, 1.0f, 0.5f, 0.1f);
    }

    void play_evolve_resonance_rate() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        for(int i=0; i<4; ++i) play_sfx_internal(108.0f / (1.0f + i * 0.2f), 0.12f, 0.4f, false, 0.999f, (float)i/3.0f, 0.02f);
    }

    void play_glitch_spawn() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(216.0f, 0.25f, 1.5f, false, 1.0f, 0.5f, 0.1f);
        play_sfx_internal(54.0f, 0.30f, 1.2f, false, 1.0f, 0.5f, 0.05f);
    }

    void play_player_damage() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(27.5f, 0.45f, 0.8f, false, 0.995f, 0.5f, 0.01f);
        play_sfx_internal(55.0f, 0.35f, 0.6f, false, 0.995f, 0.5f, 0.01f);
    }
    
    void play_dissipate() { 
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        play_sfx_internal(216.0f, 0.08f, 0.1f, false, 1.0f, 0.5f, 0.02f); 
    }

    void set_binaural(float carrier, float beat, float amp) {
        target_carrier = carrier;
        target_beat = beat;
        target_amp = amp;
    }

    void play_divine_ascent() {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
        // Clean harmonic series (1, 2, 3, 4 ratio) for a pure chime
        float harmonic_ratios[] = {1.0f, 2.0f, 3.0f, 4.0f};
        for(int i=0; i<4; ++i) {
            float f = current_carrier * harmonic_ratios[i];
            // f_mod = 1.0f (no slide), attack = 0.05s, decay = longer for resonance
            play_sfx_internal(f, 0.06f, 2.0f, false, 1.0f, 0.5f, 0.05f);
        }
    }

    struct MusicState { bool is_drop; float intensity; int movement; };
    MusicState get_music_state() { return { current_movement == THE_NEXUS || current_movement == RESOLUTION, current_intensity.load(), (int)current_movement }; }

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
    
    float drone_phases_l[8] = {0}, drone_phases_r[8] = {0}; // Replacing Organ with Drone

    float swoosh_phase_l = 0.0f, swoosh_phase_r = 0.0f;
    float reactor_phases_l[4] = {0}, reactor_phases_r[4] = {0};
    float hull_cycle_phase = 0.0f;
    float musical_sub_phase_l = 0.0f, musical_sub_phase_r = 0.0f;
    float master_gain = 1.0f;
    float dc_l = 0.0f, dc_r = 0.0f;

    static constexpr int MAX_SFX = 128;
    Voice sfx_voices[MAX_SFX];
    std::mt19937 noise_gen{std::random_device{}()};
    std::uniform_real_distribution<float> noise_dist{-1.0f, 1.0f};

    float get_saw(float& p, float f, float sr) {
        p += (f * 2.0f * std::numbers::pi_v<float>) / sr;
        if (p > std::numbers::pi_v<float>) p -= 2.0f * std::numbers::pi_v<float>;
        return p / std::numbers::pi_v<float>;
    }

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

    float soft_limit(float x) {
        float threshold = 0.75f;
        if (std::abs(x) < threshold) return x;
        if (x > 0) return threshold + (0.98f - threshold) * std::tanh((x - threshold) / (1.0f - threshold));
        return -threshold - (0.98f - threshold) * std::tanh((-x - threshold) / (1.0f - threshold));
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

                if (current_movement >= GENESIS) {
                    int step = global_step % 16;
                    int pattern_type = (global_bar / 4) % 3; 

                    bool k_hit = (step == 0 || step == 10); 
                    if (pattern_type == 1) k_hit = (step == 0 || step == 8 || step == 11); 
                    else if (pattern_type == 2) k_hit = (step == 0 || step == 3 || step == 10); 

                    if (k_hit) { kick_env = 1.0f; kick_phase_l = kick_phase_r = 0.0f; }

                    bool s_hit = (step == 4 || step == 12);
                    bool ghost = (step == 7 || step == 15 || step == 14);
                    bool fill = (global_bar % 8 == 7 && step > 12);

                    if (s_hit || ghost || fill) {
                        snare_env = s_hit ? 1.0f : (fill ? 0.65f : 0.35f);
                        snare_tone_l = snare_tone_r = snare_pop_phase = 0.0f;
                    }

                    if (step % 2 == 1) hat_env = 0.45f;

                    bool b_hit = (step == 0 || (pattern_type == 2 && step == 6));
                    if (b_hit) {
                        bass_amp = 0.90f; 
                        std::array<float, 4> progression = {1.0f, 0.75f, 1.25f, 0.9f};
                        bass_freq = current_carrier * 0.125f * progression[(global_bar / 4) % 4];
                    }

                    if (step % 8 == 0) {
                        std::array<float, 3> hive = {1.0f, 1.5f, 2.0f};
                        lead_freq = current_carrier * hive[(global_step/8) % 3];
                        lead_amp = 0.35f;
                    }
                }
            }

            float b_gamma = 40.0f; 
            float b_beta = 20.0f;  
            float p_l = std::cos(pan * (std::numbers::pi_v<float> * 0.5f));
            float p_r = std::sin(pan * (std::numbers::pi_v<float> * 0.5f));

            // Speaker-Ready Binaural Beats: Volume LFO tied to the beat frequency
            static float beat_lfo_phase = 0.0f;
            beat_lfo_phase += (b_gamma * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            float speaker_ready_lfo = 0.85f + 0.15f * std::sin(beat_lfo_phase);

            current_intensity.store(std::lerp(current_intensity.load(), kick_env * 0.5f + bass_amp * 0.5f, 0.001f));

            // --- BUS 1: SHIP ---
            float ship_l = 0, ship_r = 0;
            float rev_f = 1.0f + speed * 0.25f;
            float cycle_f = 10.0f + speed * 10.0f; 
            hull_cycle_phase += (cycle_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            float mechanical_pulse = 0.80f + 0.20f * std::sin(hull_cycle_phase);

            float s_f = 216.0f * (1.0f + speed * 0.15f);
            swoosh_phase_l += (s_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            swoosh_phase_r += ((s_f + b_beta) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            ship_l += std::sin(swoosh_phase_l) * 0.12f * speed * current_engine_vol * p_l;
            ship_r += std::sin(swoosh_phase_r) * 0.12f * speed * current_engine_vol * p_r;

            std::array<float, 4> growl_freqs = { 27.0f, 54.0f, 108.0f, 300.0f };
            std::array<float, 4> growl_amps = { 0.12f, 0.08f, 0.04f, 0.02f };
            for(int j=0; j<4; ++j) {
                float f_l = growl_freqs[j] * rev_f;
                float f_r = f_l + b_gamma;
                float w_l = std::sin(reactor_phases_l[j]);
                float w_r = std::sin(reactor_phases_r[j]);
                // Apply speaker_ready_lfo to the reactor growls
                ship_l += w_l * growl_amps[j] * current_engine_vol * mechanical_pulse * p_l * speaker_ready_lfo;
                ship_r += w_r * growl_amps[j] * current_engine_vol * mechanical_pulse * p_r * speaker_ready_lfo;
                reactor_phases_l[j] += (f_l * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                reactor_phases_r[j] += (f_r * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            }

            // --- BUS 2: ETERNAL NEXUS SYMPHONY (REBALANCED) ---
            float music_l = 0, music_r = 0;
            
            // A. ETHEREAL DRONE (Replacing Organ)
            float drone_f = current_carrier * 0.25f;
            std::array<float, 8> drone_ratios = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f};
            float drone_amp = 0.08f + current_intensity.load() * 0.05f;
            float lfo = 0.5f + 0.5f * std::sin(global_step * 0.05f); // Slow LFO for filter movement
            for(int j=0; j<8; ++j) {
                float f_l = drone_f * drone_ratios[j];
                float f_r = f_l + b_gamma;
                float harmonic_amp = drone_amp * (0.4f / (j + 1)) * (j < 4 ? 1.0f : lfo);
                // Apply speaker_ready_lfo to the ethereal drones
                music_l += std::sin(drone_phases_l[j]) * harmonic_amp * speaker_ready_lfo;
                music_r += std::sin(drone_phases_r[j]) * harmonic_amp * speaker_ready_lfo;
                drone_phases_l[j] += (f_l * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                drone_phases_r[j] += (f_r * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            }

            // B. DRUMS (The March)
            if (kick_env > 0.001f) {
                float k_f = 40.0f + 150.0f * (kick_env * kick_env);
                music_l += std::sin(kick_phase_l) * kick_env * 0.50f;
                music_r += std::sin(kick_phase_r) * kick_env * 0.50f;
                kick_phase_l += (k_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_phase_r += ((k_f + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_env *= 0.9994f;
            }
            if (snare_env > 0.001f) {
                float pop_f = 180.0f + 620.0f * (snare_env * snare_env);
                snare_pop_phase += (pop_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                float pop = std::sin(snare_pop_phase) * snare_env * 0.40f;
                snare_tone_l += (180.0f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                snare_tone_r += ((180.0f + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                float rattle = noise_dist(noise_gen) * snare_env * 0.40f;
                music_l += (pop + std::sin(snare_tone_l) * 0.20f + rattle) * 0.40f;
                music_r += (pop + std::sin(snare_tone_r) * 0.20f + rattle) * 0.40f;
                snare_env *= 0.9988f; 
            }
            if (bass_amp > 0.001f) {
                float b_l = 0, b_r = 0;
                std::array<float, 4> detunes = {1.0f, 1.002f, 0.998f, 1.005f};
                for(int j=0; j<4; ++j) {
                    b_l += get_saw(reese_phases_l[j], bass_freq * detunes[j], sample_rate);
                    b_r += get_saw(reese_phases_r[j], (bass_freq + b_gamma) * detunes[j], sample_rate);
                }
                music_l += b_l * 0.25f * bass_amp * 0.30f;
                music_r += b_r * 0.25f * bass_amp * 0.30f;
                bass_amp *= 0.99985f;
            }
            if (lead_amp > 0.001f) {
                lead_phase_l += (lead_freq * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                lead_phase_r += ((lead_freq + b_gamma) * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                music_l += std::sin(lead_phase_l) * lead_amp * 0.15f;
                music_r += std::sin(lead_phase_r) * lead_amp * 0.15f;
                lead_amp *= 0.99975f;
            }

            // --- BUS 3: SFX ---
            float sfx_l = 0, sfx_r = 0;
            for (auto& vox : sfx_voices) {
                if (vox.active) {
                    float s = vox.is_noise ? (noise_dist(noise_gen) * 0.4f) : std::sin(vox.phase);
                    s *= vox.amplitude;
                    if (vox.elapsed < vox.attack) s *= (vox.elapsed / vox.attack);
                    else if (vox.elapsed > vox.duration - vox.decay) { float rem = (vox.duration - vox.elapsed) / vox.decay; s *= std::max(0.0f, rem); }
                    sfx_l += s * (1.0f - vox.pan); sfx_r += s * vox.pan;
                    if (!vox.is_noise) { vox.phase += vox.phase_increment; vox.phase_increment *= vox.freq_mod; }
                    vox.elapsed += 1.0f / sample_rate;
                    if (vox.elapsed >= vox.duration) vox.active = false;
                }
            }

            // MASTER MIX
            float mix_l = ship_l + music_l + sfx_l;
            float mix_r = ship_r + music_r + sfx_r;

            dc_l = 0.995f * dc_l + 0.005f * mix_l;
            dc_r = 0.995f * dc_r + 0.005f * mix_r;
            mix_l -= dc_l; mix_r -= dc_r;

            float peak = std::max(std::abs(mix_l), std::abs(mix_r));
            float target_gain = (peak > 0.90f) ? (0.90f / peak) : 1.0f;
            master_gain = std::lerp(master_gain, target_gain, (target_gain < master_gain) ? 0.05f : 0.0001f);

            // NAN/INF SAFETY CHECK - CRITICAL
            if (std::isnan(mix_l) || std::isinf(mix_l)) { mix_l = 0.0f; dc_l = 0.0f; }
            if (std::isnan(mix_r) || std::isinf(mix_r)) { mix_r = 0.0f; dc_r = 0.0f; }

            buffer[i] = std::clamp(soft_limit(mix_l * master_gain * 1.3f), -0.99f, 0.99f);
            buffer[i + 1] = std::clamp(soft_limit(mix_r * master_gain * 1.3f), -0.99f, 0.99f);
        }
    }
};
