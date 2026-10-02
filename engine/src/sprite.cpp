#include "sprout/sprite.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "internal.hpp"

namespace sprout {

Sprite::Sprite(const std::string& imagePath) { setImage(imagePath); }

Sprite::Sprite(const Texture& texture) { setImage(texture); }

void Sprite::setImage(const std::string& imagePath) { setImage(loadTexture(imagePath)); }

void Sprite::setImage(const Texture& texture) {
    texture_ = texture;
    frame_ = std::clamp(frame_, 0, frameCount() - 1);
}

void Sprite::setSpriteSheet(int columns, int rows) {
    columns_ = std::max(1, columns);
    rows_ = std::max(1, rows);
    frame_ = std::clamp(frame_, 0, frameCount() - 1);
}

void Sprite::setFrame(int frame) {
    playing_ = false;
    current_.clear();
    frame_ = std::clamp(frame, 0, frameCount() - 1);
}

void Sprite::addAnimation(const std::string& name, int firstFrame, int lastFrame, float framesPerSecond, bool loop) {
    std::vector<int> frames;
    const int step = firstFrame <= lastFrame ? 1 : -1;
    for (int f = firstFrame; f != lastFrame + step; f += step) frames.push_back(f);
    addAnimation(name, std::move(frames), framesPerSecond, loop);
}

void Sprite::addAnimation(const std::string& name, std::vector<int> frames, float framesPerSecond, bool loop) {
    if (frames.empty()) {
        detail::warn("the animation \"" + name + "\" has no frames.");
        return;
    }
    animations_[name] = Animation{std::move(frames), std::max(0.01f, framesPerSecond), loop};
}

void Sprite::play(const std::string& name) {
    if (current_ == name && playing_) return;
    if (current_ == name && !isAnimationFinished()) {
        playing_ = true;
        return;
    }
    restart(name);
}

void Sprite::restart(const std::string& name) {
    auto it = animations_.find(name);
    if (it == animations_.end()) {
        detail::warn("there is no animation called \"" + name + "\". Add it with addAnimation() first.");
        return;
    }
    current_ = name;
    animIndex_ = 0;
    animTime_ = 0.0f;
    playing_ = true;
    frame_ = std::clamp(it->second.frames.front(), 0, frameCount() - 1);
}

void Sprite::stop() { playing_ = false; }

bool Sprite::isAnimationFinished() const {
    auto it = animations_.find(current_);
    if (it == animations_.end()) return false;
    const Animation& anim = it->second;
    return !anim.loop && animIndex_ >= static_cast<int>(anim.frames.size()) - 1;
}

void Sprite::update(float dt) {
    if (!playing_) return;
    auto it = animations_.find(current_);
    if (it == animations_.end()) return;
    const Animation& anim = it->second;
    const int frameCountInAnim = static_cast<int>(anim.frames.size());

    animTime_ += dt;
    const float frameTime = 1.0f / anim.fps;
    while (animTime_ >= frameTime) {
        animTime_ -= frameTime;
        if (animIndex_ + 1 < frameCountInAnim) {
            ++animIndex_;
        } else if (anim.loop) {
            animIndex_ = 0;
        } else {
            playing_ = false; // stay on the last frame
            animTime_ = 0.0f;
            break;
        }
    }
    frame_ = std::clamp(anim.frames[static_cast<std::size_t>(animIndex_)], 0, frameCount() - 1);
}

Rect Sprite::frameSource(int frame) const {
    const float frameWidth = static_cast<float>(texture_.width) / static_cast<float>(columns_);
    const float frameHeight = static_cast<float>(texture_.height) / static_cast<float>(rows_);
    const int column = frame % columns_;
    const int row = frame / columns_;
    return Rect{static_cast<float>(column) * frameWidth, static_cast<float>(row) * frameHeight, frameWidth,
                frameHeight};
}

float Sprite::width() const {
    return static_cast<float>(texture_.width) / static_cast<float>(columns_) * std::abs(scale.x);
}

float Sprite::height() const {
    return static_cast<float>(texture_.height) / static_cast<float>(rows_) * std::abs(scale.y);
}

Rect Sprite::bounds() const {
    const Vec2 drawSize = size();
    return Rect{position - anchor * drawSize, drawSize};
}

void Sprite::draw() const { draw(position); }

void Sprite::draw(Vec2 at) const {
    if (!visible || !texture_.isValid()) return;
    Rect source = frameSource(frame_);
    // A negative source size tells raylib to mirror the picture.
    if (flipX != (scale.x < 0.0f)) source.width = -source.width;
    if (flipY != (scale.y < 0.0f)) source.height = -source.height;
    const Vec2 drawSize = size();
    drawTexturePart(texture_, source, Rect{at, drawSize}, anchor * drawSize, rotation, tint);
}

} // namespace sprout
