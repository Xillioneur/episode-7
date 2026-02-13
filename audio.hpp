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

    ~AudioEngine() {
        if (device != 0) SDL_CloseAudioDevice(device);
    }

    void play_sfx(float freq, float amp, float dur, bool noise = false) {
        if (device == 0) return;
        std::lock_guard<std::mutex> lock(mtx);
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
                v.attack = 0.005f;
                v.decay = dur * 0.5f;
                break;
            }
        }
    }

    void set_binaural(float carrier, float beat, float amp) {
        target_carrier = carrier;
        target_beat = beat;
        target_amp = amp;
    }

private:
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec actual;
    std::mutex mtx;
    
    std::atomic<float> target_carrier{172.0f}; 
    std::atomic<float> target_beat{20.0f}; 
    std::atomic<float> target_amp{0.12f};

    float current_carrier = 172.0f;
    float current_beat = 20.0f;
    float current_amp = 0.0f; 
    
    float phase_l = 0.0f;
    float phase_r = 0.0f;

    // --- Drum and Bass Sequencer ---
    float bpm = 172.0f; 
    float beat_timer = 0.0f;
    int current_step = 0;
    
    float kick_amp = 0.0f;
    float kick_phase = 0.0f;
    float snare_amp = 0.0f;
    float hat_amp = 0.0f;
    
    float reese_phase1 = 0.0f;
    float reese_phase2 = 0.0f;
    float bass_amp = 0.0f;
    
    float lead_phase = 0.0f;
    float lead_amp = 0.0f;
    float lead_freq = 440.0f;

    static constexpr int MAX_SFX = 32;
    Voice sfx_voices[MAX_SFX];
    std::mt19937 noise_gen{std::random_device{}()};
    std::uniform_real_distribution<float> noise_dist{-1.0f, 1.0f};

    float synth_wave(float p, int harmonics = 2) {
        float s = 0.0f;
        for (int i = 1; i <= harmonics; ++i) {
            s += std::sin(p * i) / (float)i;
        }
        return s;
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

        float step_dur = 60.0f / (bpm * 4.0f);

        for (size_t i = 0; i < buffer.size(); i += 2) {
            beat_timer += 1.0f / sample_rate;
            if (beat_timer >= step_dur) {
                beat_timer -= step_dur;
                current_step = (current_step + 1) % 16;

                if (current_step == 0 || current_step == 6 || current_step == 9) { kick_amp = 1.0f; kick_phase = 0.0f; }
                if (current_step == 4 || current_step == 10) snare_amp = 1.0f;
                if (current_step % 2 == 1) hat_amp = 0.2f;
                if (current_step % 8 == 0) { bass_amp = 0.8f; }
                if (current_step % 2 == 0) {
                    std::array<float, 4> notes = {1.0f, 1.25f, 1.5f, 1.8f};
                    lead_freq = current_carrier * 2.0f * notes[(current_step / 2) % 4];
                    lead_amp = 0.25f;
                }
            }

            float out_l = 0.0f;
            float out_r = 0.0f;

            if (kick_amp > 0) {
                float freq = 180.0f * kick_amp;
                float s = std::sin(kick_phase) * kick_amp * 0.5f;
                out_l += s; out_r += s;
                kick_phase += (freq * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                kick_amp *= 0.9990f;
            }
            if (snare_amp > 0) {
                float s = noise_dist(noise_gen) * snare_amp * 0.25f;
                out_l += s; out_r += s;
                snare_amp *= 0.9992f;
            }
            if (hat_amp > 0) {
                float s = noise_dist(noise_gen) * hat_amp * 0.1f;
                out_l += s; out_r += s;
                hat_amp *= 0.9985f;
            }

            float bass_f = current_carrier * 0.5f;
            float b = (synth_wave(reese_phase1, 3) + synth_wave(reese_phase2, 3)) * bass_amp * 0.15f;
            out_l += b; out_r += b;
            reese_phase1 += (bass_f * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            reese_phase2 += ((bass_f + 1.5f) * 2.0f * std::numbers::pi_v<float>) / sample_rate; 
            bass_amp = std::lerp(bass_amp, 0.4f, 0.001f); 

            float gate = 0.1f + 0.9f * std::abs(std::sin(beat_timer * (bpm/60.0f) * std::numbers::pi_v<float>));
            float p_l = synth_wave(phase_l, 1) * current_amp * gate * 0.2f;
            float p_r = synth_wave(phase_r, 1) * current_amp * gate * 0.2f;
            out_l += p_l; out_r += p_r;
            phase_l += (current_carrier * 2.0f * std::numbers::pi_v<float>) / sample_rate;
            phase_r += ((current_carrier + current_beat) * 2.0f * std::numbers::pi_v<float>) / sample_rate;

            if (lead_amp > 0) {
                float s = std::sin(lead_phase) * lead_amp * 0.05f;
                out_l += s; out_r += s;
                lead_phase += (lead_freq * 2.0f * std::numbers::pi_v<float>) / sample_rate;
                lead_amp *= 0.9996f;
            }

            for (auto& vox : sfx_voices) {
                if (vox.active) {
                    float s = vox.is_noise ? noise_dist(noise_gen) * 0.3f : std::sin(vox.phase);
                    s *= vox.amplitude;
                    if (vox.elapsed < vox.attack) s *= (vox.elapsed / vox.attack);
                    else if (vox.elapsed > vox.duration - vox.decay) {
                        float rem = (vox.duration - vox.elapsed) / vox.decay;
                        s *= std::max(0.0f, rem);
                    }
                    out_l += s; out_r += s;
                    vox.phase += vox.phase_increment;
                    vox.elapsed += 1.0f / sample_rate;
                    if (vox.elapsed >= vox.duration) vox.active = false;
                }
            }

            auto wrap = [](float& p) { if (p > 2.0f * std::numbers::pi_v<float>) p -= 2.0f * std::numbers::pi_v<float>; };
            wrap(phase_l); wrap(phase_r); wrap(reese_phase1); wrap(reese_phase2); wrap(kick_phase); wrap(lead_phase);

            buffer[i] = std::clamp(out_l, -1.0f, 1.0f);
            buffer[i + 1] = std::clamp(out_r, -1.0f, 1.0f);
        }
    }
};