#pragma once
// =============================================================================
//  sprout/save.hpp - saving progress (high scores, settings, unlocked levels)
//
//    SaveFile save("savegame.json");          // loads it if it already exists
//
//    int best = save.get("highscore", 0);     // 0 if there's no high score yet
//    save.set("highscore", 1200);             // saved to the file straight away
//    save.set("playerName", "Sam");
//
//  The file is plain JSON, so you can open it in any text editor:
//    { "highscore": 1200, "playerName": "Sam" }
//
//  You can also read your own JSON files (for levels, enemy stats, ...):
//    Json data = loadJson("assets/enemies.json");
//    int hp = data["goblin"]["health"];
//
//  JSON support comes from the nlohmann/json library:
//  https://github.com/nlohmann/json
// =============================================================================
#include <string>

#include <nlohmann/json.hpp>

namespace sprout {

/// A JSON value: a number, text, true/false, a list [...] or an object {...}.
using Json = nlohmann::json;

class SaveFile {
public:
    /// Opens a save file. If it doesn't exist yet, you start with an empty one.
    /// A name like "save.json" means a file in your game's folder (next to the .exe).
    explicit SaveFile(std::string path = "save.json");

    /// Reads a value. If it's missing (or the wrong type), you get defaultValue.
    template <class T>
    T get(const std::string& key, const T& defaultValue) const {
        if (!data_.is_object() || !data_.contains(key)) return defaultValue;
        try {
            return data_.at(key).get<T>();
        } catch (const nlohmann::json::exception&) {
            return defaultValue;
        }
    }
    /// So that save.get("name", "nobody") gives you a std::string.
    std::string get(const std::string& key, const char* defaultValue) const {
        return get<std::string>(key, std::string(defaultValue));
    }

    /// Stores a value and writes the file right away.
    template <class T>
    void set(const std::string& key, const T& value) {
        data_[key] = value;
        write();
    }

    /// Is there a value with this name?
    bool has(const std::string& key) const;
    /// Deletes one value (and writes the file).
    void remove(const std::string& key);
    /// Deletes everything (and writes the file).
    void clear();
    /// Writes the file. set(), remove() and clear() already do this for you.
    bool write() const;
    /// Reads the file again.
    void reload();

    /// The whole save as JSON, for advanced use. Call write() after changing it.
    Json& data() { return data_; }
    const Json& data() const { return data_; }
    /// Where the file really is (the full path).
    const std::string& path() const { return path_; }

private:
    std::string path_;
    Json data_ = Json::object();
};

/// Reads a JSON file. If it's missing or broken you get an empty Json (null)
/// and a message in the console explaining what went wrong.
Json loadJson(const std::string& path);
/// Writes JSON to a file, nicely indented. Returns false if it failed.
bool saveJson(const std::string& path, const Json& data);

} // namespace sprout
