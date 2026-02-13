# Game Design Document: NEON NEXUS - HARMONY (C++23)

## 1. Executive Summary
**NEON NEXUS: HARMONY** is a non-violent, high-octane twin-stick "harmonizer." Set in a beautiful neon digital void, the player acts as a restorative force, using "Harmony Rays" (lasers) to stabilize "Glitches" and bring them back into the Nexus. The game uses a restorative, musical aesthetic to provide a relaxing yet challenging experience.

## 2. Technical Stack
*   **C++23 Features**: `std::generator` for spawn patterns, `std::print/format`, `std::expected`, and `std::ranges`.
*   **Architecture**: Multi-threaded with a `WorkerPool` for parallel grid physics and an `AudioEngine` running on a low-latency thread.
*   **Spatial Partitioning**: Custom `QuadTree` for efficient entity management.

## 3. Core Gameplay Mechanics (Non-Violent)
### 3.1 Restorative Combat
*   **Harmony Rays**: The player emits fast-moving beams of blue light.
*   **Glitches**: These are chaotic data fragments (formerly enemies). They come in various shapes: Basic, Dash, and Dense.
*   **Harmonization**: When a Glitch's instability is neutralized by Harmony Rays, it doesn't "die"—it harmonizes, transforming into a burst of musical notes and a "Harmony Orb."
*   **Stability & Integrity**: The player manages their "Integrity." Contact with Glitches reduces it.

### 3.2 Evolution
*   Players collect Harmony Orbs to reach new "Evolution" stages, choosing between **Ray Density**, **Harmony Power**, and **Resonance Rate**.

## 4. Audio Systems
### 4.1 Atmospheric Synthesis
*   **Synth Pad**: A rich, multi-harmonic atmospheric drone replacing pure sines.
*   **Speaker-Ready Binaural Beats**: A volume LFO tied to the beat frequency (Beta/Gamma range) to ensure the pulse is felt even on stereo speakers.
*   **Musical Feedback**: Harmonizing a glitch plays a resonant musical chime instead of an explosion sound.

## 5. Visual Style
*   **Harmony Aesthetics**: A deeper blue background with a reactive grid that pulses like a visualizer.
*   **Laser Beams**: High-speed, glowing vectors with light trails.
*   **Golden Sparkles**: Visual feedback for harmonization and integrity loss.