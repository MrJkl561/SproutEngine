#include "sprout/audio.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "internal.hpp"

namespace sprout {

namespace {

// How many copies of one sound can play at the same time.
constexpr std::size_t kMaxVoicesPerSound = 8;

// An extra "voice" that shares the sound data of the original sound.
// raylib-cpp has no wrapper for sound aliases, so this small one cleans up after itself.
struct SoundAlias {
    raylib::SoundUnmanaged sound;
    explicit SoundAlias(const ::Sound& original) : sound(::LoadSoundAlias(original)) {}
    ~SoundAlias() { ::UnloadSoundAlias(sound); }
    SoundAlias(const SoundAlias&) = delete;
    SoundAlias& operator=(const SoundAlias&) = delete;
};

struct SoundEntry {
    std::optional<raylib::Sound> original;           // empty = couldn't be loaded
    std::vector<std::unique_ptr<SoundAlias>> aliases; // pointers so the voices never move in memory
    std::size_t next = 0;
};

struct AudioState {
    // The device must outlive every sound, so it is listed first (destroyed last).
    std::optional<raylib::AudioDevice> device;
    std::map<std::string, SoundEntry> sounds;
    std::map<std::string, std::optional<raylib::Music>> music; // empty = couldn't be loaded
    std::string currentMusic;
    bool musicPaused = false;

    bool ready() const { return device.has_value(); }
};

AudioState& audio() {
    static AudioState state;
    return state;
}

SoundEntry* getSound(const std::string& path) {
    AudioState& state = audio();
    if (!state.ready()) return nullptr;
    if (auto it = state.sounds.find(path); it != state.sounds.end())
        return it->second.original ? &it->second : nullptr;

    SoundEntry& entry = state.sounds[path]; // remembered even if it fails, so we only warn once
    if (!raylib::FileExists(path)) {
        detail::warn("couldn't find the sound \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder.");
        return nullptr;
    }
    try {
        entry.original.emplace(path);
    } catch (const raylib::RaylibException&) {
        detail::warn("couldn't load the sound \"" + path + "\". Try a .wav, .ogg or .mp3 file.");
        return nullptr;
    }
    return &entry;
}

raylib::Music* getMusic(const std::string& path) {
    AudioState& state = audio();
    if (!state.ready()) return nullptr;
    if (auto it = state.music.find(path); it != state.music.end())
        return it->second ? &*it->second : nullptr;

    std::optional<raylib::Music>& slot = state.music[path];
    if (!raylib::FileExists(path)) {
        detail::warn("couldn't find the music \"" + path + "\". Check the spelling and that it is in your "
                     "game's assets folder.");
        return nullptr;
    }
    try {
        slot.emplace(path);
    } catch (const raylib::RaylibException&) {
        detail::warn("couldn't load the music \"" + path + "\". Try an .ogg or .mp3 file.");
        return nullptr;
    }
    return &*slot;
}

raylib::Music* currentMusic() {
    AudioState& state = audio();
    if (state.currentMusic.empty()) return nullptr;
    auto it = state.music.find(state.currentMusic);
    return it != state.music.end() && it->second ? &*it->second : nullptr;
}

} // namespace

namespace detail {
void initAudio() {
    try {
        audio().device.emplace(); // raylib::AudioDevice opens the sound card
    } catch (const raylib::RaylibException&) {
        warn("no sound device was found, so the game will be silent.");
    }
}

void updateAudio() {
    if (raylib::Music* music = currentMusic(); music && !audio().musicPaused) music->Update();
}

void shutdownAudio() {
    AudioState& state = audio();
    // Sounds and music unload themselves, but must go BEFORE the sound card is closed.
    state.sounds.clear();
    state.music.clear();
    state.device.reset(); // raylib::AudioDevice closes the sound card
    state.currentMusic.clear();
    state.musicPaused = false;
}
} // namespace detail

// --- Sound effects ---------------------------------------------------------------
void playSound(const std::string& path, float volume, float pitch) {
    SoundEntry* entry = getSound(path);
    if (!entry) return;

    // Find a voice that isn't busy, so the same sound can overlap itself.
    raylib::SoundUnmanaged* voice = nullptr;
    if (!entry->original->IsPlaying()) {
        voice = &*entry->original;
    } else {
        for (auto& alias : entry->aliases)
            if (!alias->sound.IsPlaying()) { voice = &alias->sound; break; }
        if (!voice && entry->aliases.size() + 1 < kMaxVoicesPerSound) {
            entry->aliases.push_back(std::make_unique<SoundAlias>(*entry->original));
            voice = &entry->aliases.back()->sound;
        }
        if (!voice) { // all voices busy: cut off the oldest one
            entry->next = (entry->next + 1) % (entry->aliases.size() + 1);
            voice = entry->next == 0 ? &*entry->original : &entry->aliases[entry->next - 1]->sound;
        }
    }
    voice->SetVolume(std::clamp(volume, 0.0f, 1.0f));
    voice->SetPitch(std::max(0.01f, pitch));
    voice->Play();
}

void stopSound(const std::string& path) {
    AudioState& state = audio();
    auto it = state.sounds.find(path);
    if (it == state.sounds.end() || !it->second.original) return;
    it->second.original->Stop();
    for (auto& alias : it->second.aliases) alias->sound.Stop();
}

bool isSoundPlaying(const std::string& path) {
    AudioState& state = audio();
    auto it = state.sounds.find(path);
    if (it == state.sounds.end() || !it->second.original) return false;
    if (it->second.original->IsPlaying()) return true;
    return std::any_of(it->second.aliases.begin(), it->second.aliases.end(),
                       [](const auto& alias) { return alias->sound.IsPlaying(); });
}

void preloadSound(const std::string& path) { getSound(path); }

// --- Music -------------------------------------------------------------------------
void playMusic(const std::string& path, float volume, bool loop) {
    AudioState& state = audio();
    raylib::Music* music = getMusic(path);
    if (!music) return;
    music->looping = loop;
    music->SetVolume(std::clamp(volume, 0.0f, 1.0f));
    if (state.currentMusic == path && music->IsPlaying()) return;
    if (state.musicPaused && state.currentMusic == path) {
        resumeMusic();
        return;
    }
    if (raylib::Music* old = currentMusic(); old && old != music) old->Stop();
    state.currentMusic = path;
    state.musicPaused = false;
    music->Play();
}

void stopMusic() {
    if (raylib::Music* music = currentMusic()) music->Stop();
    audio().currentMusic.clear();
    audio().musicPaused = false;
}

void pauseMusic() {
    if (raylib::Music* music = currentMusic()) {
        music->Pause();
        audio().musicPaused = true;
    }
}

void resumeMusic() {
    if (raylib::Music* music = currentMusic()) {
        music->Resume();
        audio().musicPaused = false;
    }
}

bool isMusicPlaying() {
    raylib::Music* music = currentMusic();
    return music && !audio().musicPaused && music->IsPlaying();
}

void setMusicVolume(float volume) {
    if (raylib::Music* music = currentMusic()) music->SetVolume(std::clamp(volume, 0.0f, 1.0f));
}

void setMasterVolume(float volume) {
    if (audio().ready()) audio().device->SetVolume(std::clamp(volume, 0.0f, 1.0f));
}

float masterVolume() { return audio().ready() ? ::GetMasterVolume() : 0.0f; }

} // namespace sprout
