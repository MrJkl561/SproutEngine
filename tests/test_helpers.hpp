#pragma once
// Small helpers shared by the test files.
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <sprout/color.hpp>
#include <sprout/math.hpp>
#include <sprout/tilemap.hpp>

// --- Pretty printing ------------------------------------------------------------------
// So that a failing CHECK(a == b) prints "Vec2{1, 2}" instead of "{?}".
namespace doctest {
template <>
struct StringMaker<sprout::Vec2> {
    static String convert(const sprout::Vec2& v) {
        std::ostringstream out;
        out << "Vec2{" << v.x << ", " << v.y << "}";
        return out.str().c_str();
    }
};
template <>
struct StringMaker<sprout::Rect> {
    static String convert(const sprout::Rect& r) {
        std::ostringstream out;
        out << "Rect{" << r.x << ", " << r.y << ", " << r.width << ", " << r.height << "}";
        return out.str().c_str();
    }
};
template <>
struct StringMaker<sprout::Color> {
    static String convert(const sprout::Color& c) {
        std::ostringstream out;
        out << "Color{" << int(c.r) << ", " << int(c.g) << ", " << int(c.b) << ", " << int(c.a) << "}";
        return out.str().c_str();
    }
};
template <>
struct StringMaker<sprout::TileCoord> {
    static String convert(const sprout::TileCoord& c) {
        std::ostringstream out;
        out << "TileCoord{column " << c.column << ", row " << c.row << "}";
        return out.str().c_str();
    }
};
} // namespace doctest

namespace sprout_test {

/// Like doctest::Approx, but for a whole Vec2:   CHECK(v == ApproxVec2(1, 2));
struct ApproxVec2 {
    ApproxVec2(double xValue, double yValue, double tolerance = 1e-4) : x(xValue), y(yValue), epsilon(tolerance) {}
    double x, y, epsilon;
};

inline bool approxEqual(double a, double b, double epsilon) {
    return std::abs(a - b) <= epsilon * (1.0 + std::max(std::abs(a), std::abs(b)));
}
inline bool operator==(const sprout::Vec2& v, const ApproxVec2& expected) {
    return approxEqual(v.x, expected.x, expected.epsilon) && approxEqual(v.y, expected.y, expected.epsilon);
}

/// Catches everything printed to std::cerr while it exists (Sprout prints its
/// friendly warnings there), so tests can check that a warning was printed
/// without spamming the test output.
class CaptureStderr {
public:
    CaptureStderr() : old_(std::cerr.rdbuf(buffer_.rdbuf())) {}
    ~CaptureStderr() { std::cerr.rdbuf(old_); }
    CaptureStderr(const CaptureStderr&) = delete;
    CaptureStderr& operator=(const CaptureStderr&) = delete;

    std::string text() const { return buffer_.str(); }

    /// How many times `needle` appears in what was printed.
    int count(const std::string& needle) const {
        const std::string all = text();
        int found = 0;
        for (std::size_t at = all.find(needle); at != std::string::npos; at = all.find(needle, at + needle.size()))
            ++found;
        return found;
    }

private:
    std::ostringstream buffer_;
    std::streambuf* old_;
};

/// A fresh, empty folder inside the system's temp folder. It is deleted
/// (with everything in it) when the TempDir goes away.
class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("sprout_tests_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    /// The full path of a file inside this folder (as a std::string, like the engine wants).
    std::string file(const std::string& name) const { return (path_ / name).string(); }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

/// Writes `contents` to a file exactly as given (binary mode, so "\r\n" stays "\r\n").
inline void writeFile(const std::string& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
}

/// Reads a whole file into a string ("" if it can't be opened).
inline std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

} // namespace sprout_test

namespace doctest {
template <>
struct StringMaker<sprout_test::ApproxVec2> {
    static String convert(const sprout_test::ApproxVec2& v) {
        std::ostringstream out;
        out << "Approx Vec2{" << v.x << ", " << v.y << "}";
        return out.str().c_str();
    }
};
} // namespace doctest
