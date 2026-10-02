#include "sprout/audio.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "internal.hpp"

namespace sprout {

namespace {

// How many copies of one sound can play at the same time.
constexpr std::size_t kMaxVoicesPerSound = 8;

struct SoundEntry {
    ::Sound original{};
    std::vector<::Sound> aliases; // extra "voices" sharing the same sound data
    std::size_t next = 0;
};

struct MusicEntry {
    ::Music music{};
};

struct AudioState {
    bool ready = false;
    std::map<std::string, SoundEntry> sounds;
    std::map<std::string, MusicEntry> music;
    std::string currentMusic;
    bool musicPaused = false;
};

AudioState& audio() {
    static AudioState state;
    return state;
}

SoundEntry* getSound(const std::string& path) {
    AudioState& state = audio();
    if (!state.ready) return nullptr;
    if (auto it = state.sounds.find(path); it != state.sounds.end()) return &it->second;

    if (!FileExists(path.c_str())) {
        detail::warn("couldn't find the sound \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder.");
        state.sounds[path] = SoundEntry{}; // remember, so we only warn once
        return nullptr;
    }
    ::Sound sound = LoadSound(path.c_str());
    if (!IsSoundValid(sound)) {
        detail::warn("couldn't load the sound \"" + path + "\". Try a .wav, .ogg or .mp3 file.");
        state.sounds[path] = SoundEntry{};
        return nullptr;
    }
    SoundEntry& entry = state.sounds[path];
    entry.original = sound;
    return &entry;
}

MusicEntry* getMusic(const std::string& path) {
    AudioState& state = audio();
    if (!state.ready) return nullptr;
    if (auto it = state.music.find(path); it != state.music.end())
        return it->second.music.frameCount > 0 ? &it->second : nullptr;

    if (!FileExists(path.c_str())) {
        detail::warn("couldn't find the music \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder.");
        state.music[path] = MusicEntry{};
        return nullptr;
    }
    ::Music music = LoadMusicStream(path.c_str());
    if (!IsMusicValid(music)) {
        detail::warn("couldn't load the music \"" + path + "\". Try an .ogg or .mp3 file.");
        state.music[path] = MusicEntry{};
        return nullptr;
    }
    state.music[path].music = music;
    return &state.music[path];
}

MusicEntry* currentMusic() {
    AudioState& state = audio();
    if (state.currentMusic.empty()) return nullptr;
    auto it = state.music.find(state.currentMusic);
    return it != state.music.end() && it->second.music.frameCount > 0 ? &it->second : nullptr;
}

} // namespace

namespace detail {
void initAudio() {
    InitAudioDevice();
    audio().ready = IsAudioDeviceReady();
    if (!audio().ready) warn("no sound device was found, so the game will be silent.");
}

void updateAudio() {
    AudioState& state = audio();
    if (MusicEntry* entry = currentMusic(); entry && !state.musicPaused) UpdateMusicStream(entry->music);
}

void shutdownAudio() {
    AudioState& state = audio();
    if (state.ready) {
        for (auto& [path, entry] : state.sounds) {
            for (::Sound& alias : entry.aliases) UnloadSoundAlias(alias);
            if (entry.original.frameCount > 0) UnloadSound(entry.original);
        }
        for (auto& [path, entry] : state.music)
            if (entry.music.frameCount > 0) UnloadMusicStream(entry.music);
        CloseAudioDevice();
    }
    state = AudioState{};
}
} // namespace detail

// --- Sound effects ---------------------------------------------------------------
void playSound(const std::string& path, float volume, float pitch) {
    SoundEntry* entry = getSound(path);
    if (!entry) return;

    // Find a voice that isn't busy, so the same sound can overlap itself.
    ::Sound* voice = nullptr;
    if (!IsSoundPlaying(entry->original)) {
        voice = &entry->original;
    } else {
        for (::Sound& alias : entry->aliases)
            if (!IsSoundPlaying(alias)) { voice = &alias; break; }
        if (!voice && entry->aliases.size() + 1 < kMaxVoicesPerSound) {
            entry->aliases.push_back(LoadSoundAlias(entry->original));
            voice = &entry->aliases.back();
        }
        if (!voice) { // all voices busy: cut off the oldest one
            entry->next = (entry->next + 1) % (entry->aliases.size() + 1);
            voice = entry->next == 0 ? &entry->original : &entry->aliases[entry->next - 1];
        }
    }
    SetSoundVolume(*voice, std::clamp(volume, 0.0f, 1.0f));
    SetSoundPitch(*voice, std::max(0.01f, pitch));
    PlaySound(*voice);
}

void stopSound(const std::string& path) {
    AudioState& state = audio();
    auto it = state.sounds.find(path);
    if (!state.ready || it == state.sounds.end() || it->second.original.frameCount == 0) return;
    StopSound(it->second.original);
    for (::Sound& alias : it->second.aliases) StopSound(alias);
}

bool isSoundPlaying(const std::string& path) {
    AudioState& state = audio();
    auto it = state.sounds.find(path);
    if (!state.ready || it == state.sounds.end() || it->second.original.frameCount == 0) return false;
    if (IsSoundPlaying(it->second.original)) return true;
    return std::any_of(it->second.aliases.begin(), it->second.aliases.end(),
                       [](const ::Sound& alias) { return IsSoundPlaying(alias); });
}

void preloadSound(const std::string& path) { getSound(path); }

// --- Music -------------------------------------------------------------------------
void playMusic(const std::string& path, float volume, bool loop) {
    AudioState& state = audio();
    MusicEntry* entry = getMusic(path);
    if (!entry) return;
    entry->music.looping = loop;
    SetMusicVolume(entry->music, std::clamp(volume, 0.0f, 1.0f));
    if (state.currentMusic == path && IsMusicStreamPlaying(entry->music)) return;
    if (state.musicPaused && state.currentMusic == path) {
        resumeMusic();
        return;
    }
    if (MusicEntry* old = currentMusic(); old && old != entry) StopMusicStream(old->music);
    state.currentMusic = path;
    state.musicPaused = false;
    PlayMusicStream(entry->music);
}

void stopMusic() {
    if (MusicEntry* entry = currentMusic()) StopMusicStream(entry->music);
    audio().currentMusic.clear();
    audio().musicPaused = false;
}

void pauseMusic() {
    if (MusicEntry* entry = currentMusic()) {
        PauseMusicStream(entry->music);
        audio().musicPaused = true;
    }
}

void resumeMusic() {
    if (MusicEntry* entry = currentMusic()) {
        ResumeMusicStream(entry->music);
        audio().musicPaused = false;
    }
}

bool isMusicPlaying() {
    MusicEntry* entry = currentMusic();
    return entry && !audio().musicPaused && IsMusicStreamPlaying(entry->music);
}

void setMusicVolume(float volume) {
    if (MusicEntry* entry = currentMusic()) SetMusicVolume(entry->music, std::clamp(volume, 0.0f, 1.0f));
}

void setMasterVolume(float volume) {
    if (audio().ready) SetMasterVolume(std::clamp(volume, 0.0f, 1.0f));
}

float masterVolume() { return audio().ready ? GetMasterVolume() : 0.0f; }

} // namespace sprout
