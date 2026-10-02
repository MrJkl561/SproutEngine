#include "sprout/save.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include "internal.hpp"

namespace sprout {

namespace {
// A relative path like "save.json" always means "in the game's folder" - even for a
// SaveFile made before the Game (e.g. a global one), when the working folder may
// still be somewhere else.
std::string inGameFolder(std::string path) {
    const std::filesystem::path p{path};
    if (p.empty() || p.is_absolute()) return path;
    return (std::filesystem::path{::GetApplicationDirectory()} / p).lexically_normal().string();
}
} // namespace

SaveFile::SaveFile(std::string path) : path_(inGameFolder(std::move(path))) { reload(); }

void SaveFile::reload() {
    std::ifstream file(path_);
    if (!file) { // no save yet - that's fine, start empty
        data_ = Json::object();
        return;
    }
    try {
        data_ = Json::parse(file);
        if (!data_.is_object()) {
            detail::warn("the save file \"" + path_ + "\" doesn't contain a JSON object, starting fresh.");
            data_ = Json::object();
        }
    } catch (const Json::parse_error& error) {
        detail::warn("the save file \"" + path_ + "\" is broken (" + error.what() + "), starting fresh.");
        data_ = Json::object();
    }
}

bool SaveFile::has(const std::string& key) const { return data_.is_object() && data_.contains(key); }

void SaveFile::remove(const std::string& key) {
    data_.erase(key);
    write();
}

void SaveFile::clear() {
    data_ = Json::object();
    write();
}

bool SaveFile::write() const { return saveJson(path_, data_); }

Json loadJson(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        detail::warn("couldn't open the JSON file \"" + path + "\". Check the spelling and that it exists.");
        return Json{};
    }
    try {
        return Json::parse(file);
    } catch (const Json::parse_error& error) {
        detail::warn("the JSON file \"" + path + "\" has a mistake in it: " + error.what());
        return Json{};
    }
}

bool saveJson(const std::string& path, const Json& data) {
    // Turn the JSON into text BEFORE touching the file, so a problem can never
    // leave a half-written (or empty) save behind. Text that isn't valid UTF-8
    // (e.g. "Ren\xE9" from a file saved in an old Windows encoding) gets a
    // replacement character instead of throwing an exception.
    const std::string text = data.dump(4, ' ', false, Json::error_handler_t::replace) + '\n';

    // Write to a temporary file first, then swap it in: if the computer crashes
    // halfway through, the old save is still there.
    const std::string temporary = path + ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary);
        if (!file || !(file << text) || !file.flush()) {
            detail::warn("couldn't write the file \"" + path + "\".");
            return false;
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        detail::warn("couldn't write the file \"" + path + "\".");
        return false;
    }
    return true;
}

} // namespace sprout
