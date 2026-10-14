#include "tiled_import.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <map>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "tilemap.hpp"

namespace fs = std::filesystem;

namespace s2d {

using nlohmann::json;

namespace {

// ---------------------------------------------------------------------------
// A small XML reader: enough for Tiled's files (elements, attributes, text).
// ---------------------------------------------------------------------------
struct Xml {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::string text;
    std::vector<Xml> kids;

    bool has(const std::string& key) const {
        for (auto& [k, v] : attrs)
            if (k == key) return true;
        return false;
    }
    std::string attr(const std::string& key, const std::string& fallback = "") const {
        for (auto& [k, v] : attrs)
            if (k == key) return v;
        return fallback;
    }
    double num(const std::string& key, double fallback = 0) const {
        std::string v = attr(key);
        return v.empty() ? fallback : std::strtod(v.c_str(), nullptr);
    }
    const Xml* child(const std::string& n) const {
        for (auto& k : kids)
            if (k.name == n) return &k;
        return nullptr;
    }
};

std::string decodeEntities(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        size_t semi = s.find(';', i);
        if (semi == std::string::npos) {
            out += s[i];
            continue;
        }
        std::string e = s.substr(i + 1, semi - i - 1);
        if (e == "amp") out += '&';
        else if (e == "lt") out += '<';
        else if (e == "gt") out += '>';
        else if (e == "quot") out += '"';
        else if (e == "apos") out += '\'';
        else if (!e.empty() && e[0] == '#') {
            long cp = e.size() > 1 && (e[1] == 'x' || e[1] == 'X') ? std::strtol(e.c_str() + 2, nullptr, 16)
                                                                   : std::strtol(e.c_str() + 1, nullptr, 10);
            int len = 0;
            const char* utf8 = CodepointToUTF8(static_cast<int>(cp), &len);
            out.append(utf8, static_cast<size_t>(len));
        } else {
            out += "&" + e + ";";
        }
        i = semi;
    }
    return out;
}

class XmlParser {
public:
    explicit XmlParser(const std::string& s) : s_(s) {}

    bool parse(Xml& root, std::string& error) {
        skipMisc();
        if (!element(root)) {
            error = error_.empty() ? "not an XML file" : error_;
            return false;
        }
        return true;
    }

private:
    void skipSpace() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
    }
    bool starts(const char* p) const { return s_.compare(i_, std::strlen(p), p) == 0; }
    // Declarations, comments and doctypes between elements
    void skipMisc() {
        for (;;) {
            skipSpace();
            if (starts("<?")) {
                size_t e = s_.find("?>", i_);
                i_ = e == std::string::npos ? s_.size() : e + 2;
            } else if (starts("<!--")) {
                size_t e = s_.find("-->", i_);
                i_ = e == std::string::npos ? s_.size() : e + 3;
            } else if (starts("<!") && !starts("<![CDATA[")) {
                size_t e = s_.find('>', i_);
                i_ = e == std::string::npos ? s_.size() : e + 1;
            } else {
                return;
            }
        }
    }
    std::string nameToken() {
        size_t b = i_;
        while (i_ < s_.size() && !std::isspace(static_cast<unsigned char>(s_[i_])) && s_[i_] != '>' && s_[i_] != '/' &&
               s_[i_] != '=')
            ++i_;
        return s_.substr(b, i_ - b);
    }
    bool element(Xml& out) {
        if (i_ >= s_.size() || s_[i_] != '<') return false;
        ++i_;
        out.name = nameToken();
        if (out.name.empty()) return false;
        // Attributes
        for (;;) {
            skipSpace();
            if (i_ >= s_.size()) return fail("the file ends inside <" + out.name + ">");
            if (starts("/>")) {
                i_ += 2;
                return true;
            }
            if (s_[i_] == '>') {
                ++i_;
                break;
            }
            std::string key = nameToken();
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != '=') return fail("an attribute without a value in <" + out.name + ">");
            ++i_;
            skipSpace();
            if (i_ >= s_.size() || (s_[i_] != '"' && s_[i_] != '\'')) return fail("an unquoted attribute in <" + out.name + ">");
            char q = s_[i_++];
            size_t e = s_.find(q, i_);
            if (e == std::string::npos) return fail("an unfinished attribute in <" + out.name + ">");
            out.attrs.push_back({key, decodeEntities(s_.substr(i_, e - i_))});
            i_ = e + 1;
        }
        // Children and text, up to </name>
        for (;;) {
            if (i_ >= s_.size()) return fail("<" + out.name + "> is never closed");
            if (starts("</")) {
                size_t e = s_.find('>', i_);
                if (e == std::string::npos) return fail("an unfinished closing tag");
                i_ = e + 1;
                return true;
            }
            if (starts("<!--") || starts("<?")) {
                skipMisc();
                continue;
            }
            if (starts("<![CDATA[")) {
                size_t e = s_.find("]]>", i_);
                if (e == std::string::npos) return fail("an unfinished CDATA section");
                out.text += s_.substr(i_ + 9, e - i_ - 9);
                i_ = e + 3;
                continue;
            }
            if (s_[i_] == '<') {
                Xml kid;
                if (!element(kid)) return false;
                out.kids.push_back(std::move(kid));
                continue;
            }
            size_t e = s_.find('<', i_);
            if (e == std::string::npos) e = s_.size();
            out.text += decodeEntities(s_.substr(i_, e - i_));
            i_ = e;
        }
    }
    bool fail(const std::string& why) {
        if (error_.empty()) error_ = why;
        return false;
    }

    const std::string& s_;
    size_t i_ = 0;
    std::string error_;
};

bool readFile(const fs::path& file, std::string& out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ---------------------------------------------------------------------------
// The map, as read from either kind of file
// ---------------------------------------------------------------------------
using Props = std::map<std::string, std::string>;

bool truthy(const Props& p, std::initializer_list<const char*> names) {
    for (const char* n : names) {
        auto it = p.find(n);
        if (it != p.end() && (it->second == "true" || it->second == "1")) return true;
    }
    return false;
}

struct Tileset {
    int firstGid = 1;
    std::string name;
    fs::path image;  // full path
    int tileWidth = 16, tileHeight = 16, margin = 0, spacing = 0, columns = 0, count = 0;
    int imageWidth = 0, imageHeight = 0;
    bool collection = false;   // a picture per tile (not supported)
    bool allSolid = false;
    std::set<int> solid;
    std::map<int, std::string> types;
    bool animated = false;
};

struct Object {
    int id = 0;
    std::string name, type;
    double x = 0, y = 0, width = 0, height = 0, rotation = 0;
    uint32_t gid = 0;
    bool visible = true, point = false, ellipse = false, shape = false;  // shape: polygon or polyline
    bool isText = false;
    std::string text;
    int pixelSize = 16;
    Color textColor = BLACK;
    Props props;
};

struct Layer {
    enum Kind { Tiles, Objects, Image, Group } kind = Tiles;
    std::string name, type;
    bool visible = true;
    double opacity = 1, offsetX = 0, offsetY = 0;
    Color tint = WHITE;
    Props props;
    struct Cell {
        int x, y;
        uint32_t gid;
    };
    std::vector<Cell> cells;
    std::vector<Object> objects;
    fs::path image;
    std::vector<Layer> layers;
};

struct Map {
    std::string orientation = "orthogonal";
    int tileWidth = 16, tileHeight = 16;
    std::vector<Tileset> tilesets;
    std::vector<Layer> layers;
};

constexpr uint32_t kFlipH = 0x80000000u, kFlipV = 0x40000000u, kFlipD = 0x20000000u, kGidMask = 0x0FFFFFFFu;

// "#AARRGGBB" or "#RRGGBB"
Color parseColor(const std::string& text, Color fallback) {
    std::string h = text;
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    if (h.size() != 6 && h.size() != 8) return fallback;
    unsigned long v = std::strtoul(h.c_str(), nullptr, 16);
    if (h.size() == 6) return Color{(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, 255};
    return Color{(unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v, (unsigned char)(v >> 24)};
}

// --- Tile data: CSV, or base64 (maybe zlib/gzip compressed) ------------------
std::vector<unsigned char> base64(const std::string& text) {
    static int table[256];
    static bool made = false;
    if (!made) {
        for (int& t : table) t = -1;
        const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int k = 0; k < 64; ++k) table[static_cast<unsigned char>(a[k])] = k;
        made = true;
    }
    std::vector<unsigned char> out;
    int bits = 0, value = 0;
    for (unsigned char c : text) {
        if (table[c] < 0) continue;  // whitespace, '=' padding
        value = (value << 6) | table[c];
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((value >> bits) & 0xFF));
        }
    }
    return out;
}

bool inflate(std::vector<unsigned char>& data, const std::string& compression, std::string& error) {
    if (compression.empty()) return true;
    size_t start = 0;
    if (compression == "zlib") {
        if (data.size() < 6 || (data[0] & 0x0F) != 8) {
            error = "the tile data isn't valid zlib";
            return false;
        }
        start = 2;
    } else if (compression == "gzip") {
        if (data.size() < 18 || data[0] != 0x1f || data[1] != 0x8b) {
            error = "the tile data isn't valid gzip";
            return false;
        }
        int flags = data[3];
        start = 10;
        if (flags & 4) start += 2 + (data[start] | (data[start + 1] << 8));  // extra field
        if (flags & 8) while (start < data.size() && data[start++]) {}       // file name
        if (flags & 16) while (start < data.size() && data[start++]) {}      // comment
        if (flags & 2) start += 2;                                           // header checksum
    } else {
        error = "tile data compressed with " + compression +
                " can't be read: in Tiled, set the map's Tile Layer Format to CSV or Base64 (zlib) and save again";
        return false;
    }
    if (start >= data.size()) {
        error = "the tile data is empty";
        return false;
    }
    int size = 0;
    unsigned char* raw = DecompressData(data.data() + start, static_cast<int>(data.size() - start), &size);
    if (!raw || size <= 0) {
        if (raw) MemFree(raw);
        error = "the tile data couldn't be decompressed";
        return false;
    }
    data.assign(raw, raw + size);
    MemFree(raw);
    return true;
}

bool gidsFromBytes(const std::vector<unsigned char>& data, std::vector<uint32_t>& out) {
    for (size_t k = 0; k + 3 < data.size(); k += 4)
        out.push_back(static_cast<uint32_t>(data[k]) | (static_cast<uint32_t>(data[k + 1]) << 8) |
                      (static_cast<uint32_t>(data[k + 2]) << 16) | (static_cast<uint32_t>(data[k + 3]) << 24));
    return true;
}

bool gidsFromCsv(const std::string& text, std::vector<uint32_t>& out) {
    const char* p = text.c_str();
    while (*p) {
        while (*p && !std::isdigit(static_cast<unsigned char>(*p))) ++p;
        if (!*p) break;
        char* end = nullptr;
        out.push_back(static_cast<uint32_t>(std::strtoul(p, &end, 10)));
        p = end;
    }
    return true;
}

bool decodeText(const std::string& text, const std::string& encoding, const std::string& compression,
                std::vector<uint32_t>& out, std::string& error) {
    if (encoding == "csv") return gidsFromCsv(text, out);
    if (encoding == "base64") {
        std::vector<unsigned char> data = base64(text);
        if (!inflate(data, compression, error)) return false;
        return gidsFromBytes(data, out);
    }
    error = "unknown tile data encoding '" + encoding + "'";
    return false;
}

void addCells(Layer& layer, const std::vector<uint32_t>& gids, int x0, int y0, int width) {
    if (width <= 0) return;
    for (size_t k = 0; k < gids.size(); ++k)
        if (gids[k] & kGidMask)
            layer.cells.push_back({x0 + static_cast<int>(k) % width, y0 + static_cast<int>(k) / width, gids[k]});
}

// --- TMX (XML) ---------------------------------------------------------------
Props xmlProps(const Xml& e) {
    Props out;
    if (const Xml* ps = e.child("properties"))
        for (const Xml& p : ps->kids)
            if (p.name == "property") out[p.attr("name")] = p.has("value") ? p.attr("value") : p.text;
    return out;
}

bool tilesetFromXml(const Xml& t, const fs::path& dir, Tileset& ts, std::string& error);
bool tilesetFromJson(const json& t, const fs::path& dir, Tileset& ts, std::string& error);

bool loadExternalTileset(const fs::path& file, Tileset& ts, std::string& error) {
    std::string text;
    if (!readFile(file, text)) {
        error = "can't open the tile set " + file.string();
        return false;
    }
    std::string ext = lower(file.extension().string());
    if (ext == ".tsx" || ext == ".xml") {
        Xml x;
        XmlParser parser(text);
        if (!parser.parse(x, error)) {
            error = file.filename().string() + ": " + error;
            return false;
        }
        return tilesetFromXml(x, file.parent_path(), ts, error);
    }
    try {
        return tilesetFromJson(json::parse(text), file.parent_path(), ts, error);
    } catch (const json::exception& e) {
        error = file.filename().string() + ": " + e.what();
        return false;
    }
}

bool tilesetFromXml(const Xml& t, const fs::path& dir, Tileset& ts, std::string& error) {
    if (t.has("source")) return loadExternalTileset(dir / t.attr("source"), ts, error);
    ts.name = t.attr("name");
    ts.tileWidth = static_cast<int>(t.num("tilewidth", 16));
    ts.tileHeight = static_cast<int>(t.num("tileheight", 16));
    ts.margin = static_cast<int>(t.num("margin"));
    ts.spacing = static_cast<int>(t.num("spacing"));
    ts.columns = static_cast<int>(t.num("columns"));
    ts.count = static_cast<int>(t.num("tilecount"));
    ts.allSolid = truthy(xmlProps(t), {"solid", "collision", "collides", "collidable"});
    if (const Xml* img = t.child("image")) {
        ts.image = dir / img->attr("source");
        ts.imageWidth = static_cast<int>(img->num("width"));
        ts.imageHeight = static_cast<int>(img->num("height"));
    } else {
        ts.collection = true;
    }
    for (const Xml& tile : t.kids) {
        if (tile.name != "tile") continue;
        int id = static_cast<int>(tile.num("id"));
        std::string type = tile.has("class") ? tile.attr("class") : tile.attr("type");
        if (!type.empty()) ts.types[id] = type;
        const Xml* shapes = tile.child("objectgroup");
        if ((shapes && !shapes->kids.empty()) || truthy(xmlProps(tile), {"solid", "collision", "collides", "collidable"}))
            ts.solid.insert(id);
        if (tile.child("animation")) ts.animated = true;
    }
    return true;
}

Object objectFromXml(const Xml& o) {
    Object ob;
    ob.id = static_cast<int>(o.num("id"));
    ob.name = o.attr("name");
    ob.type = o.has("class") ? o.attr("class") : o.attr("type");
    ob.x = o.num("x");
    ob.y = o.num("y");
    ob.width = o.num("width");
    ob.height = o.num("height");
    ob.rotation = o.num("rotation");
    ob.gid = static_cast<uint32_t>(std::strtoul(o.attr("gid", "0").c_str(), nullptr, 10));
    ob.visible = o.attr("visible", "1") != "0";
    ob.point = o.child("point") != nullptr;
    ob.ellipse = o.child("ellipse") != nullptr;
    ob.shape = o.child("polygon") || o.child("polyline");
    if (const Xml* t = o.child("text")) {
        ob.isText = true;
        ob.text = t->text;
        ob.pixelSize = static_cast<int>(t->num("pixelsize", 16));
        ob.textColor = parseColor(t->attr("color"), BLACK);
    }
    ob.props = xmlProps(o);
    return ob;
}

void layerCommonXml(const Xml& e, Layer& l) {
    l.name = e.attr("name");
    l.type = e.has("class") ? e.attr("class") : e.attr("type");
    l.visible = e.attr("visible", "1") != "0";
    l.opacity = e.num("opacity", 1);
    l.offsetX = e.num("offsetx");
    l.offsetY = e.num("offsety");
    l.tint = parseColor(e.attr("tintcolor"), WHITE);
    l.props = xmlProps(e);
}

bool layersFromXml(const Xml& parent, const fs::path& dir, std::vector<Layer>& out, std::string& error) {
    for (const Xml& e : parent.kids) {
        Layer l;
        if (e.name == "layer") {
            l.kind = Layer::Tiles;
            layerCommonXml(e, l);
            const Xml* data = e.child("data");
            if (!data) continue;
            std::string enc = data->attr("encoding"), comp = data->attr("compression");
            int width = static_cast<int>(e.num("width"));
            auto read = [&](const Xml& from, int x0, int y0, int w) {
                std::vector<uint32_t> gids;
                if (enc.empty()) {  // the old style: a <tile gid="..."/> each
                    for (const Xml& t : from.kids)
                        if (t.name == "tile") gids.push_back(static_cast<uint32_t>(std::strtoul(t.attr("gid", "0").c_str(), nullptr, 10)));
                } else if (!decodeText(from.text, enc, comp, gids, error)) {
                    return false;
                }
                addCells(l, gids, x0, y0, w);
                return true;
            };
            bool chunks = false;
            for (const Xml& c : data->kids) {
                if (c.name != "chunk") continue;
                chunks = true;
                if (!read(c, static_cast<int>(c.num("x")), static_cast<int>(c.num("y")), static_cast<int>(c.num("width"))))
                    return false;
            }
            if (!chunks && !read(*data, 0, 0, width)) return false;
        } else if (e.name == "objectgroup") {
            l.kind = Layer::Objects;
            layerCommonXml(e, l);
            for (const Xml& o : e.kids)
                if (o.name == "object") l.objects.push_back(objectFromXml(o));
        } else if (e.name == "imagelayer") {
            l.kind = Layer::Image;
            layerCommonXml(e, l);
            if (const Xml* img = e.child("image")) l.image = dir / img->attr("source");
        } else if (e.name == "group") {
            l.kind = Layer::Group;
            layerCommonXml(e, l);
            if (!layersFromXml(e, dir, l.layers, error)) return false;
        } else {
            continue;
        }
        out.push_back(std::move(l));
    }
    return true;
}

bool mapFromXml(const Xml& m, const fs::path& dir, Map& map, std::string& error) {
    if (m.name != "map") {
        error = "this isn't a Tiled map (no <map>)";
        return false;
    }
    map.orientation = m.attr("orientation", "orthogonal");
    map.tileWidth = static_cast<int>(m.num("tilewidth", 16));
    map.tileHeight = static_cast<int>(m.num("tileheight", 16));
    for (const Xml& t : m.kids) {
        if (t.name != "tileset") continue;
        Tileset ts;
        ts.firstGid = static_cast<int>(t.num("firstgid", 1));
        if (!tilesetFromXml(t, dir, ts, error)) return false;
        map.tilesets.push_back(ts);
    }
    return layersFromXml(m, dir, map.layers, error);
}

// --- TMJ (JSON) ----------------------------------------------------------------
Props jsonProps(const json& e) {
    Props out;
    if (e.contains("properties") && e["properties"].is_array())
        for (const json& p : e["properties"]) {
            if (!p.contains("name") || !p.contains("value")) continue;
            const json& v = p["value"];
            out[p["name"].get<std::string>()] = v.is_string() ? v.get<std::string>() : v.dump();
        }
    return out;
}

std::string jsonClass(const json& e) {
    if (e.contains("class") && e["class"].is_string()) return e["class"].get<std::string>();
    if (e.contains("type") && e["type"].is_string()) return e["type"].get<std::string>();
    return "";
}

bool tilesetFromJson(const json& t, const fs::path& dir, Tileset& ts, std::string& error) {
    if (t.contains("source")) return loadExternalTileset(dir / t["source"].get<std::string>(), ts, error);
    ts.name = t.value("name", std::string());
    ts.tileWidth = t.value("tilewidth", 16);
    ts.tileHeight = t.value("tileheight", 16);
    ts.margin = t.value("margin", 0);
    ts.spacing = t.value("spacing", 0);
    ts.columns = t.value("columns", 0);
    ts.count = t.value("tilecount", 0);
    ts.allSolid = truthy(jsonProps(t), {"solid", "collision", "collides", "collidable"});
    if (t.contains("image") && t["image"].is_string() && !t["image"].get<std::string>().empty()) {
        ts.image = dir / t["image"].get<std::string>();
        ts.imageWidth = t.value("imagewidth", 0);
        ts.imageHeight = t.value("imageheight", 0);
    } else {
        ts.collection = true;
    }
    if (t.contains("tiles") && t["tiles"].is_array()) {
        for (const json& tile : t["tiles"]) {
            int id = tile.value("id", 0);
            std::string type = jsonClass(tile);
            if (!type.empty()) ts.types[id] = type;
            bool shapes = tile.contains("objectgroup") && tile["objectgroup"].contains("objects") &&
                          !tile["objectgroup"]["objects"].empty();
            if (shapes || truthy(jsonProps(tile), {"solid", "collision", "collides", "collidable"})) ts.solid.insert(id);
            if (tile.contains("animation")) ts.animated = true;
        }
    }
    return true;
}

void layerCommonJson(const json& e, Layer& l) {
    l.name = e.value("name", std::string());
    l.type = e.value("class", std::string());
    l.visible = e.value("visible", true);
    l.opacity = e.value("opacity", 1.0);
    l.offsetX = e.value("offsetx", 0.0);
    l.offsetY = e.value("offsety", 0.0);
    l.tint = parseColor(e.value("tintcolor", std::string()), WHITE);
    l.props = jsonProps(e);
}

bool jsonData(const json& holder, const std::string& enc, const std::string& comp, std::vector<uint32_t>& out,
              std::string& error) {
    if (!holder.contains("data")) return true;
    const json& d = holder["data"];
    if (d.is_array()) {
        for (const json& g : d) out.push_back(g.get<uint32_t>());
        return true;
    }
    if (d.is_string()) return decodeText(d.get<std::string>(), enc.empty() ? "csv" : enc, comp, out, error);
    return true;
}

bool layersFromJson(const json& list, const fs::path& dir, std::vector<Layer>& out, std::string& error) {
    if (!list.is_array()) return true;
    for (const json& e : list) {
        std::string type = e.value("type", std::string());
        Layer l;
        layerCommonJson(e, l);
        if (type == "tilelayer") {
            l.kind = Layer::Tiles;
            std::string enc = e.value("encoding", std::string()), comp = e.value("compression", std::string());
            if (e.contains("chunks") && e["chunks"].is_array()) {
                for (const json& c : e["chunks"]) {
                    std::vector<uint32_t> gids;
                    if (!jsonData(c, enc, comp, gids, error)) return false;
                    addCells(l, gids, c.value("x", 0), c.value("y", 0), c.value("width", 0));
                }
            } else {
                std::vector<uint32_t> gids;
                if (!jsonData(e, enc, comp, gids, error)) return false;
                addCells(l, gids, 0, 0, e.value("width", 0));
            }
        } else if (type == "objectgroup") {
            l.kind = Layer::Objects;
            if (e.contains("objects"))
                for (const json& o : e["objects"]) {
                    Object ob;
                    ob.id = o.value("id", 0);
                    ob.name = o.value("name", std::string());
                    ob.type = jsonClass(o);
                    ob.x = o.value("x", 0.0);
                    ob.y = o.value("y", 0.0);
                    ob.width = o.value("width", 0.0);
                    ob.height = o.value("height", 0.0);
                    ob.rotation = o.value("rotation", 0.0);
                    ob.gid = o.value("gid", 0u);
                    ob.visible = o.value("visible", true);
                    ob.point = o.value("point", false);
                    ob.ellipse = o.value("ellipse", false);
                    ob.shape = o.contains("polygon") || o.contains("polyline");
                    if (o.contains("text") && o["text"].is_object()) {
                        ob.isText = true;
                        ob.text = o["text"].value("text", std::string());
                        ob.pixelSize = o["text"].value("pixelsize", 16);
                        ob.textColor = parseColor(o["text"].value("color", std::string()), BLACK);
                    }
                    ob.props = jsonProps(o);
                    l.objects.push_back(ob);
                }
        } else if (type == "imagelayer") {
            l.kind = Layer::Image;
            std::string img = e.value("image", std::string());
            if (!img.empty()) l.image = dir / img;
        } else if (type == "group") {
            l.kind = Layer::Group;
            if (e.contains("layers") && !layersFromJson(e["layers"], dir, l.layers, error)) return false;
        } else {
            continue;
        }
        out.push_back(std::move(l));
    }
    return true;
}

bool mapFromJson(const json& m, const fs::path& dir, Map& map, std::string& error) {
    if (!m.is_object() || m.value("type", std::string()) != "map") {
        error = "this isn't a Tiled map (its \"type\" isn't \"map\")";
        return false;
    }
    map.orientation = m.value("orientation", std::string("orthogonal"));
    map.tileWidth = m.value("tilewidth", 16);
    map.tileHeight = m.value("tileheight", 16);
    if (m.contains("tilesets"))
        for (const json& t : m["tilesets"]) {
            Tileset ts;
            ts.firstGid = t.value("firstgid", 1);
            if (!tilesetFromJson(t, dir, ts, error)) return false;
            map.tilesets.push_back(ts);
        }
    if (m.contains("layers") && !layersFromJson(m["layers"], dir, map.layers, error)) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Making the nodes
// ---------------------------------------------------------------------------
class Builder {
public:
    Builder(const Map& map, const fs::path& projectDir, TiledImport& result)
        : map_(map), project_(projectDir), result_(result) {}

    std::unique_ptr<Node> build(const std::string& name) {
        auto root = createNode("Node2D");
        root->name = name;
        for (const Tileset& ts : map_.tilesets) {
            if (ts.collection)
                warn("The tile set '" + ts.name + "' is a collection of separate pictures; only tile sets made from one picture can be imported, so its tiles were left out.");
            if (ts.animated) warn("Tile animations in '" + ts.name + "' aren't imported: the first frame shows.");
        }
        if (map_.orientation != "orthogonal")
            warn("The map is " + map_.orientation + "; it was imported as an ordinary square grid.");
        for (const Layer& l : map_.layers) addLayer(*root, l, 1.0);
        return root;
    }

private:
    void warn(const std::string& w) {
        if (std::find(result_.warnings.begin(), result_.warnings.end(), w) == result_.warnings.end())
            result_.warnings.push_back(w);
    }

    // A picture as a path inside the project, copying it in if it's outside.
    std::string projectPath(const fs::path& file) {
        std::error_code ec;
        fs::path full = fs::weakly_canonical(file, ec);
        fs::path proj = fs::weakly_canonical(project_, ec);
        auto it = paths_.find(full.string());
        if (it != paths_.end()) return it->second;
        std::string out;
        fs::path rel = full.lexically_relative(proj);
        if (!rel.empty() && *rel.begin() != "..") {
            out = rel.generic_string();
        } else if (fs::exists(full, ec)) {
            fs::path dir = project_ / "assets" / "tiles";
            fs::create_directories(dir, ec);
            fs::path target = dir / full.filename();
            for (int n = 2; fs::exists(target, ec) && fs::file_size(target, ec) != fs::file_size(full, ec); ++n)
                target = dir / (full.stem().string() + "_" + std::to_string(n) + full.extension().string());
            out = fs::path(target).lexically_relative(project_).generic_string();
            if (!fs::exists(target, ec)) {
                fs::copy_file(full, target, ec);
                result_.copied.push_back(out);
            }
        } else {
            warn("The picture " + file.string() + " is missing.");
            out = file.filename().generic_string();
        }
        paths_[full.string()] = out;
        return out;
    }

    // The tile set a gid belongs to, or nullptr
    const Tileset* tilesetFor(uint32_t gid, int& index) const {
        const Tileset* found = nullptr;
        for (size_t k = 0; k < map_.tilesets.size(); ++k) {
            if (static_cast<uint32_t>(map_.tilesets[k].firstGid) <= gid &&
                (!found || map_.tilesets[k].firstGid > found->firstGid)) {
                found = &map_.tilesets[k];
                index = static_cast<int>(k);
            }
        }
        return found;
    }

    static std::string uniqueName(Node& parent, std::string name) {
        if (name.empty()) name = "Node";
        auto taken = [&](const std::string& n) {
            for (auto& c : parent.children())
                if (c->name == n) return true;
            return false;
        };
        if (!taken(name)) return name;
        for (int n = 2;; ++n)
            if (!taken(name + std::to_string(n))) return name + std::to_string(n);
    }

    static Color tinted(Color tint, double opacity) {
        tint.a = static_cast<unsigned char>(std::clamp(tint.a * opacity, 0.0, 255.0));
        return tint;
    }

    static bool namedSolid(const std::string& s) {
        std::string n = lower(s);
        return n == "solid" || n == "collision" || n == "collisions" || n == "collider" || n == "wall" || n == "walls";
    }

    void addLayer(Node& parent, const Layer& l, double opacity) {
        double op = opacity * l.opacity;
        std::unique_ptr<Node> node;
        switch (l.kind) {
            case Layer::Tiles: node = tileLayer(l, op); break;
            case Layer::Objects: node = objectLayer(l); break;
            case Layer::Image: {
                auto sprite = std::make_unique<Sprite2D>();
                if (!l.image.empty()) sprite->texture = projectPath(l.image);
                sprite->centered = false;
                sprite->modulate = tinted(l.tint, op);
                node = std::move(sprite);
                break;
            }
            case Layer::Group: {
                node = createNode("Node2D");
                for (const Layer& inner : l.layers) addLayer(*node, inner, op);
                break;
            }
        }
        if (!node) return;
        auto* n2 = node->as<Node2D>();
        n2->position = raylib::Vector2(static_cast<float>(l.offsetX), static_cast<float>(l.offsetY));
        n2->visible = l.visible;
        node->name = uniqueName(parent, l.name.empty() ? "Layer" : l.name);
        parent.addChild(std::move(node));
    }

    std::unique_ptr<Node> tileLayer(const Layer& l, double opacity) {
        auto map = std::make_unique<TileMap>();
        map->cellSize = raylib::Vector2(static_cast<float>(map_.tileWidth), static_cast<float>(map_.tileHeight));
        map->modulate = tinted(l.tint, opacity);
        bool layerSolid = namedSolid(l.name) || namedSolid(l.type) || truthy(l.props, {"solid", "collision", "collides", "collidable"});
        std::map<int, int> sourceOf;  // tile set index -> this map's source
        for (const Layer::Cell& c : l.cells) {
            uint32_t gid = c.gid & kGidMask;
            int index = -1;
            const Tileset* ts = tilesetFor(gid, index);
            if (!ts || ts->collection) continue;
            auto it = sourceOf.find(index);
            if (it == sourceOf.end()) {
                int s = map->addSource(projectPath(ts->image), ts->tileWidth, ts->tileHeight, ts->margin, ts->spacing);
                map->sources[s].solid = ts->solid;
                map->sources[s].types = ts->types;
                it = sourceOf.emplace(index, s).first;
            }
            int tile = static_cast<int>(gid) - ts->firstGid;
            if (ts->allSolid || layerSolid) map->sources[it->second].solid.insert(tile);
            int flags = ((c.gid & kFlipH) ? TileMap::FlipH : 0) | ((c.gid & kFlipV) ? TileMap::FlipV : 0) |
                        ((c.gid & kFlipD) ? TileMap::FlipDiagonal : 0);
            map->setCell(c.x, c.y, tile, it->second, flags);
        }
        return map;
    }

    std::unique_ptr<Node> objectLayer(const Layer& l) {
        auto group = createNode("Node2D");
        bool layerSolid = namedSolid(l.name) || namedSolid(l.type) || truthy(l.props, {"solid", "collision", "collides", "collidable"});
        for (const Object& o : l.objects) {
            std::unique_ptr<Node> node;
            raylib::Vector2 at(static_cast<float>(o.x), static_cast<float>(o.y));
            float w = static_cast<float>(o.width), h = static_cast<float>(o.height);
            if (o.gid & kGidMask) {
                // A tile placed as an object: a sprite, from its bottom-left corner
                uint32_t gid = o.gid & kGidMask;
                int index = -1;
                const Tileset* ts = tilesetFor(gid, index);
                if (ts && !ts->collection) {
                    auto sprite = std::make_unique<Sprite2D>();
                    sprite->texture = projectPath(ts->image);
                    if (ts->margin || ts->spacing)
                        warn("Tile objects from '" + ts->name + "' (it has a margin or spacing) may show the wrong part of the picture.");
                    int cols = ts->columns > 0 ? ts->columns : 1;
                    int count = ts->count > 0 ? ts->count : cols;
                    sprite->hframes = cols;
                    sprite->vframes = std::max(1, (count + cols - 1) / cols);
                    sprite->frame = static_cast<int>(gid) - ts->firstGid;
                    sprite->centered = false;
                    sprite->offset = raylib::Vector2(0, static_cast<float>(-ts->tileHeight));
                    if (w > 0 && h > 0)
                        sprite->scale = raylib::Vector2(w / ts->tileWidth, h / ts->tileHeight);
                    sprite->flipH = (o.gid & kFlipH) != 0;
                    sprite->flipV = (o.gid & kFlipV) != 0;
                    node = std::move(sprite);
                }
            } else if (o.isText) {
                auto label = std::make_unique<Label>();
                label->text = o.text;
                label->fontSize = o.pixelSize;
                label->color = o.textColor;
                node = std::move(label);
            } else if (!o.point && !o.shape && w > 0 && h > 0) {
                // A rectangle (or ellipse): something to bump into or walk into
                bool solid = layerSolid || namedSolid(o.type) || namedSolid(o.name) ||
                             truthy(o.props, {"solid", "collision", "collides", "collidable"});
                node = createNode(solid ? "StaticBody2D" : "Area2D");
                auto shape = std::make_unique<CollisionShape2D>();
                shape->name = "CollisionShape2D";
                shape->size = raylib::Vector2(w, h);
                shape->position = raylib::Vector2(w / 2, h / 2);
                node->addChild(std::move(shape));
            }
            if (!node) node = createNode("Node2D");  // a point, polygon or line: marks the spot
            auto* n2 = node->as<Node2D>();
            n2->position = at;
            n2->rotation = static_cast<float>(o.rotation);
            n2->visible = o.visible;
            std::string name = !o.name.empty() ? o.name : !o.type.empty() ? o.type : "Object" + std::to_string(o.id);
            node->name = uniqueName(*group, name);
            group->addChild(std::move(node));
        }
        return group;
    }

    const Map& map_;
    fs::path project_;
    TiledImport& result_;
    std::map<std::string, std::string> paths_;
};

}  // namespace

bool isTiledMap(const fs::path& file) {
    std::string ext = lower(file.extension().string());
    return ext == ".tmx" || ext == ".tmj";
}

TiledImport importTiledMap(const fs::path& mapFile, const fs::path& projectDir) {
    TiledImport result;
    std::string text;
    if (!readFile(mapFile, text)) {
        result.error = "can't open " + mapFile.string();
        return result;
    }
    Map map;
    fs::path dir = mapFile.parent_path();
    std::string ext = lower(mapFile.extension().string());
    bool ok = false;
    if (ext == ".tmx" || (!text.empty() && text.find('<') < text.find('{'))) {
        Xml root;
        XmlParser parser(text);
        ok = parser.parse(root, result.error) && mapFromXml(root, dir, map, result.error);
    } else {
        try {
            ok = mapFromJson(json::parse(text), dir, map, result.error);
        } catch (const json::exception& e) {
            result.error = e.what();
        }
    }
    if (!ok) return result;
    Builder builder(map, projectDir, result);
    result.root = builder.build(mapFile.stem().string());
    return result;
}

}  // namespace s2d
