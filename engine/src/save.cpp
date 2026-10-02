#include "sprout/save.hpp"

#include <fstream>
#include <utility>

#include "internal.hpp"

namespace sprout {

SaveFile::SaveFile(std::string path) : path_(std::move(path)) { reload(); }

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
    std::ofstream file(path);
    if (!file) {
        detail::warn("couldn't write the file \"" + path + "\".");
        return false;
    }
    file << data.dump(4) << '\n';
    return static_cast<bool>(file);
}

} // namespace sprout
