#pragma once
// =============================================================================
//  sprout/audio.hpp - sound effects and music
//
//    playSound("assets/jump.wav");            // a short sound effect
//    playSound("assets/coin.wav", 0.5f);      // at half volume
//    playSound("assets/hit.wav", 1.0f, randomFloat(0.9f, 1.1f)); // vary the pitch
//
//    playMusic("assets/song.ogg");            // background music, loops forever
//    stopMusic();
//
//  Files are loaded the first time you use them and then remembered.
//  Supported: .wav .ogg .mp3 .flac (and .qoa)
//  If the computer has no sound card, these quietly do nothing.
// =============================================================================
#include <string>

namespace sprout {

// --- Sound effects -------------------------------------------------------------
/// Plays a sound effect. The same sound can overlap itself (great for rapid fire).
/// volume: 0 = silent, 1 = normal.  pitch: 1 = normal, 2 = higher/faster, 0.5 = lower/slower.
void playSound(const std::string& path, float volume = 1.0f, float pitch = 1.0f);
/// Stops every copy of this sound that is currently playing.
void stopSound(const std::string& path);
/// Is this sound playing right now?
bool isSoundPlaying(const std::string& path);
/// Loads a sound now, so there's no tiny pause the first time it plays.
void preloadSound(const std::string& path);

// --- Music (long sounds that stream from the file) -------------------------------
/// Starts background music. Only one song plays at a time; a new song replaces the old one.
/// Calling it again with the song that's already playing does nothing (it won't restart).
void playMusic(const std::string& path, float volume = 1.0f, bool loop = true);
void stopMusic();
void pauseMusic();
void resumeMusic();
bool isMusicPlaying();
void setMusicVolume(float volume);

/// Volume of EVERYTHING (0 = mute, 1 = normal).
void setMasterVolume(float volume);
float masterVolume();

} // namespace sprout
