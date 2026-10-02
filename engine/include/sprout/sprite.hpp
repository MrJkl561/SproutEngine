#pragma once
// =============================================================================
//  sprout/sprite.hpp - pictures that move, spin, flip and animate
//
//    Sprite player("assets/player.png");
//    player.position = {400, 300};    // where its MIDDLE goes
//    player.scale = {2, 2};           // twice as big
//    player.draw();
//
//  Animation from a sprite sheet (one picture with many frames in a grid):
//
//    player.setSpriteSheet(4, 2);              // 4 columns, 2 rows = 8 frames
//    player.addAnimation("walk", 0, 3, 10);    // frames 0-3 at 10 frames/second
//    player.addAnimation("jump", 4, 7, 12, false); // play once, don't loop
//    player.play("walk");
//    // every frame:
//    player.update(dt);
//    player.draw();
//
//  Frames are numbered left-to-right, top-to-bottom, starting at 0.
// =============================================================================
#include <map>
#include <string>
#include <vector>

#include "sprout/color.hpp"
#include "sprout/graphics.hpp"
#include "sprout/math.hpp"

namespace sprout {

class Sprite {
public:
    Vec2 position;            ///< where the sprite's anchor (its middle, by default) is
    Vec2 scale{1.0f, 1.0f};   ///< 1 = normal size, 2 = double size
    float rotation = 0.0f;    ///< in degrees, clockwise
    Vec2 anchor{0.5f, 0.5f};  ///< which point is "the position": {0,0} top-left, {0.5,0.5} middle
    bool flipX = false;       ///< mirror left-right (e.g. when walking left)
    bool flipY = false;       ///< mirror upside-down
    Color tint = Color::White;///< multiply the colors (White = unchanged)
    bool visible = true;

    Sprite() = default;
    /// Makes a sprite from a picture file.
    explicit Sprite(const std::string& imagePath);
    explicit Sprite(const Texture& texture);

    /// Changes the picture.
    void setImage(const std::string& imagePath);
    void setImage(const Texture& texture);
    const Texture& texture() const { return texture_; }

    /// Cuts the picture into a grid of equally sized frames.
    void setSpriteSheet(int columns, int rows = 1);
    /// Shows one frame of the sprite sheet (stops any animation).
    void setFrame(int frame);
    int frame() const { return frame_; }
    int frameCount() const { return columns_ * rows_; }

    /// Adds an animation using frames first..last (both included).
    void addAnimation(const std::string& name, int firstFrame, int lastFrame, float framesPerSecond,
                      bool loop = true);
    /// Adds an animation from a list of frames, e.g. {0, 1, 2, 1}.
    void addAnimation(const std::string& name, std::vector<int> frames, float framesPerSecond,
                      bool loop = true);
    /// Starts an animation. If it is already the current animation it keeps going
    /// (doesn't restart), so it's fine to call play() every frame. A play-once
    /// animation that has finished stays on its last frame.
    void play(const std::string& name);
    /// Starts an animation from its first frame, even if it was already playing or finished.
    void restart(const std::string& name);
    void stop();
    bool isPlaying() const { return playing_; }
    /// The name of the current animation ("" if none).
    const std::string& animation() const { return current_; }
    /// True once a non-looping animation has reached its last frame.
    bool isAnimationFinished() const;

    /// Moves the animation forward. Call this once per frame.
    void update(float dt);
    /// Draws the sprite at `position`.
    void draw() const;
    /// Draws the sprite somewhere else (handy inside an Entity: sprite.draw(position)).
    void draw(Vec2 at) const;

    /// Size of one frame on screen (after scaling).
    float width() const;
    float height() const;
    Vec2 size() const { return {width(), height()}; }
    /// The rectangle the sprite covers (ignores rotation).
    Rect bounds() const;

private:
    struct Animation {
        std::vector<int> frames;
        float fps = 10.0f;
        bool loop = true;
    };

    Rect frameSource(int frame) const;

    Texture texture_;
    int columns_ = 1;
    int rows_ = 1;
    int frame_ = 0;
    std::map<std::string, Animation> animations_;
    std::string current_;
    int animIndex_ = 0;
    float animTime_ = 0.0f;
    bool playing_ = false;
};

} // namespace sprout
